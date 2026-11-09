#include <errno.h>
#include <string.h>
#include "ring.h"

#define META_BODY 36 /* "LOSM" gen tp tail_seq acked boot version maxrec, then CRC + commit */
#define META_VERSION 2

static void wr32(uint8_t *p, uint32_t v)
{
	for (int i = 0; i < 4; i++) {
		p[i] = v >> (8 * i);
	}
}

static uint32_t rd32(const uint8_t *p)
{
	return p[0] | p[1] << 8 | p[2] << 16 | (uint32_t)p[3] << 24;
}

static uint64_t up(uint64_t x, uint64_t a)
{
	return (x + a - 1) / a * a;
}

static uint64_t region(const struct ring *r)
{
	return (uint64_t)r->nblk * r->d->block;
}

static uint32_t phys(const struct ring *r, uint64_t x)
{
	return 2 * r->d->block + (uint32_t)(x % region(r));
}

static uint32_t rsize(const struct ring *r, uint32_t len)
{
	return up(chunk_size(len), r->d->page);
}

static void fill(uint8_t *dst, size_t off, size_t n, size_t base, const uint8_t *src, size_t len)
{
	size_t lo = off > base ? off : base;
	size_t hi = off + n < base + len ? off + n : base + len;

	if (lo < hi) {
		memcpy(dst + (lo - off), src + (lo - base), hi - lo);
	}
}

/* Programs a || b || CRC32 || commit starting at addr, one page at a time, commit last.
 * Devices that can reprogram a page get the commit word as a separate second program;
 * single-program devices (NAND) get it in the same final page program. */
static int put_stream(const struct ring_dev *d, uint32_t addr, const uint8_t *a, size_t an,
		      const uint8_t *b, size_t bn)
{
	uint8_t tr[8];
	size_t total = an + bn + 8, off = 0, n, cw = total - 4;
	uint32_t crc = chunk_crc32(chunk_crc32(0, a, an), b, bn);
	int err;

	wr32(tr, crc);
	memcpy(tr + 4, chunk_commit_word, 4);
	for (; off < total; off += d->page) {
		n = total - off < d->page ? total - off : d->page;
		memset(d->pbuf, 0xff, d->page);
		fill(d->pbuf, off, n, 0, a, an);
		fill(d->pbuf, off, n, an, b, bn);
		fill(d->pbuf, off, n, an + bn, tr, 8);
		if (off + n == total && !d->single_program) {
			memset(d->pbuf + (cw - off), 0xff, 4);
		}
		err = d->write(d->ctx, addr + off, d->pbuf, d->page);
		if (err) {
			return err;
		}
	}
	if (!d->single_program) {
		size_t po = cw - (off - d->page); /* commit offset inside the last page */
		size_t lo = po / d->wunit * d->wunit, hi = up(po + 4, d->wunit);

		memcpy(d->pbuf + po, chunk_commit_word, 4);
		return d->write(d->ctx, addr + (off - d->page) + lo, d->pbuf + lo, hi - lo);
	}
	return 0;
}

/* ---- metadata: two blocks, written alternately, highest valid generation wins ---- */

struct meta {
	uint32_t gen, tail_seq, acked, boot, version, maxrec;
	uint64_t tp;
};

static int meta_commit(struct ring *r)
{
	const struct ring_dev *d = r->d;
	uint8_t m[META_BODY];
	uint8_t slot = r->slot ^ 1;
	int err;

	memcpy(m, "LOSM", 4);
	wr32(m + 4, r->gen + 1);
	wr32(m + 8, (uint32_t)r->tp);
	wr32(m + 12, (uint32_t)(r->tp >> 32));
	wr32(m + 16, r->tail_seq);
	wr32(m + 20, r->acked);
	wr32(m + 24, r->boot);
	wr32(m + 28, META_VERSION);
	wr32(m + 32, r->maxrec);
	err = d->erase(d->ctx, slot * d->block);
	if (!err) {
		err = put_stream(d, slot * d->block, m, sizeof(m), NULL, 0);
	}
	if (!err) {
		r->gen++;
		r->slot = slot;
	}
	return err;
}

static int meta_read(const struct ring *r, uint8_t slot, struct meta *o)
{
	const struct ring_dev *d = r->d;
	uint8_t m[META_BODY + 8];

	if (d->read(d->ctx, slot * d->block, m, sizeof(m))) {
		return -EIO;
	}
	if (memcmp(m, "LOSM", 4)) {
		return -ENODATA; /* blank or never formatted */
	}
	/* An old v1 body (28 B + CRC at 28 + commit at 32) also starts with LOSM. Report it as a
	 * format mismatch rather than blank so store_init does not erase it. A torn/corrupt v2 slot
	 * is merely invalid so the other A/B slot can still be used after a power cut. */
	if (rd32(m + META_BODY) != chunk_crc32(0, m, META_BODY) ||
	    !chunk_commit_ok(m + META_BODY + 4)) {
		if (rd32(m + 28) == chunk_crc32(0, m, 28) && chunk_commit_ok(m + 32)) {
			return -EPROTO;
		}
		return -EBADMSG;
	}
	o->gen = rd32(m + 4);
	o->tp = rd32(m + 8) | (uint64_t)rd32(m + 12) << 32;
	o->tail_seq = rd32(m + 16);
	o->acked = rd32(m + 20);
	o->boot = rd32(m + 24);
	o->version = rd32(m + 28);
	o->maxrec = rd32(m + 32);
	if (o->version != META_VERSION) {
		return -EPROTO;
	}
	if (o->maxrec != r->maxrec) {
		return -EPROTO; /* never truncate/reformat a ring built with different record geometry */
	}
	return 0;
}

/* ---- records ---- */

/* Header and commit word look right for seq at x. Cheap: no payload read. */
static int rec_at(const struct ring *r, uint64_t x, uint32_t seq, uint32_t *len)
{
	uint8_t h[CHUNK_HDR_SIZE], w[4];
	uint32_t s, l;
	uint64_t R = region(r);
	uint32_t a = phys(r, x);

	if (r->d->read(r->d->ctx, a, h, sizeof(h))) {
		return -EIO;
	}
	if (chunk_hdr_peek(h, &s, &l) || s != seq || l % 4 || chunk_size(l) > r->maxrec ||
	    x % R + rsize(r, l) > R) {
		return -ENOENT;
	}
	if (r->d->read(r->d->ctx, a + CHUNK_HDR_SIZE + l + 4, w, 4)) {
		return -EIO;
	}
	if (!chunk_commit_ok(w)) {
		return -ENOENT;
	}
	*len = l;
	return 0;
}

static int verify_full(const struct ring *r, uint64_t x, uint32_t len)
{
	uint8_t b[128];
	uint32_t crc = 0, total = CHUNK_HDR_SIZE + len, a = phys(r, x), n;

	for (uint32_t off = 0; off < total; off += n) {
		n = total - off < sizeof(b) ? total - off : sizeof(b);
		if (r->d->read(r->d->ctx, a + off, b, n)) {
			return -EIO;
		}
		crc = chunk_crc32(crc, b, n);
	}
	if (r->d->read(r->d->ctx, a + total, b, 8)) {
		return -EIO;
	}
	return rd32(b) == crc && chunk_commit_ok(b + 4) ? 0 : -EBADMSG;
}

static int page_blank(const struct ring *r, uint64_t x)
{
	uint8_t buf[16];

	for (uint32_t i = 0; i < r->d->page; i += sizeof(buf)) {
		uint32_t n = r->d->page - i < sizeof(buf) ? r->d->page - i : sizeof(buf);

		if (r->d->read(r->d->ctx, phys(r, x) + i, buf, n)) {
			return -EIO;
		}
		for (uint32_t j = 0; j < n; j++) {
			if (buf[j] != 0xff) {
				return 0;
			}
		}
	}
	return 1;
}

/* Where does record seq start, given the previous record ended at e? Normally at e. After a
 * wrap or a crash the writer skipped to a later block boundary, so try those too. Stale
 * records from an earlier lap have older sequence numbers and never match. */
static int probe(const struct ring *r, uint64_t e, uint32_t seq, uint64_t limit, uint64_t *x,
		 uint32_t *len)
{
	uint64_t B = r->d->block, c = e;

	for (uint32_t j = 0; j < r->probe && c < limit; j++) {
		int err = rec_at(r, c, seq, len);

		if (!err) {
			*x = c;
			return 0;
		}
		if (err == -EIO) {
			return -EIO;
		}
		c = j ? c + B : (e / B + 1) * B;
	}
	return -ENOENT;
}

static int drop_oldest(struct ring *r)
{
	uint32_t len, nlen;
	uint64_t nx;

	if (r->tail_seq == r->next_seq) {
		return 0;
	}
	if (rec_at(r, r->tp, r->tail_seq, &len)) {
		return -EIO;
	}
	if (r->tail_seq + 1 == r->next_seq) {
		r->tp = r->wp;
	} else if (probe(r, r->tp + rsize(r, len), r->tail_seq + 1, r->wp, &nx, &nlen)) {
		return -EIO;
	} else {
		r->tp = nx;
	}
	r->tail_seq++;
	return 0;
}

static int init(struct ring *r, const struct ring_dev *d, uint32_t maxrec)
{
	if (d->page < CHUNK_HDR_SIZE || d->page % 4 || d->page % d->wunit || d->block % d->page ||
	    d->size / d->block < 4) {
		return -EINVAL;
	}
	memset(r, 0, sizeof(*r));
	r->d = d;
	r->nblk = d->size / d->block - 2;
	r->maxrec = up(maxrec, d->page);
	r->probe = r->maxrec / d->block + 3;
	return r->maxrec * 2 <= region(r) ? 0 : -EINVAL;
}

int ring_format(struct ring *r, const struct ring_dev *d, uint32_t maxrec)
{
	int err = init(r, d, maxrec);

	for (uint32_t b = 0; !err && b < d->size / d->block; b++) {
		err = d->erase(d->ctx, b * d->block);
	}
	if (err) {
		return err;
	}
	r->tail_seq = r->next_seq = 1;
	r->blk_ready = r->nblk;
	r->slot = 1;
	return meta_commit(r);
}

int ring_open(struct ring *r, const struct ring_dev *d, uint32_t maxrec)
{
	struct meta m[2], *best = NULL;
	uint64_t x, e, B = d->block;
	uint64_t lx = 0;
	uint32_t len, llen = 0, lseq = 0, seq;
	int err = init(r, d, maxrec), have = 0;

	if (err) {
		return err;
	}
	for (uint8_t s = 0; s < 2; s++) {
		int err = meta_read(r, s, &m[s]);

		if (err == -EIO || err == -EPROTO) {
			return err; /* never reformat on a read error or a different ring format */
		}
		if (!err && (!best || m[s].gen > best->gen)) {
			best = &m[s];
			r->slot = s;
		}
	}
	if (!best) {
		return -ENODATA;
	}
	r->gen = best->gen;
	r->tp = e = best->tp;
	r->tail_seq = seq = best->tail_seq;
	r->acked = best->acked;
	r->boot = best->boot;

	/* ponytail: linear walk from the tail (two small reads per record). Premium NAND with a
	 * full 516 MB backlog is ~16k records; add a checkpoint or binary search if boot time matters. */
	int pr;

	while ((pr = probe(r, e, seq, UINT64_MAX, &x, &len)) == 0) {
		if (!have) {
			r->tp = x;
		}
		have = 1;
		lx = x;
		llen = len;
		lseq = seq++;
		e = x + rsize(r, len);
	}
	if (pr == -EIO) {
		return -EIO;
	}
	if (have) {
		int vf = verify_full(r, lx, llen);

		if (vf == -EIO) {
			return -EIO;
		}
		if (vf == -EBADMSG) { /* newest record is torn or corrupt: drop it */
			e = lx;
			seq = lseq;
		}
	}
	r->next_seq = seq;
	if (seq == r->tail_seq) {
		r->tp = e;
	}
	/* Resume after e. Inside a block we can only reuse the rest if it is still blank;
	 * a torn record may have dirtied it, and blocks are only erased as a whole. */
	r->wp = e;
	for (uint64_t p = e; e % B && p < up(e, B); p += d->page) {
		int blank = page_blank(r, p);

		if (blank < 0) {
			return -EIO;
		}
		if (!blank) {
			r->wp = up(e, B);
			break;
		}
	}
	r->blk_ready = r->wp % B ? r->wp / B + 1 : r->wp / B;
	r->boot++;
	return meta_commit(r);
}

int ring_append(struct ring *r, struct chunk_hdr *h, const uint8_t *payload)
{
	const struct ring_dev *d = r->d;
	uint64_t B = d->block, R = region(r), k0, k1, start = r->wp;
	uint32_t s = rsize(r, h->len);
	uint8_t hb[CHUNK_HDR_SIZE];
	uint64_t old_tp = r->tp;
	uint32_t old_tail = r->tail_seq;
	int err, moved = 0;

	if (h->len % 4 || chunk_size(h->len) > r->maxrec) {
		return -EINVAL;
	}
	if (start % R + s > R) { /* a record never straddles the end of the region */
		start = (start / R + 1) * R;
	}
	if (r->tail_seq == r->next_seq) {
		r->tp = start;
	}
	k1 = (start + s - 1) / B;
	while (k1 - r->tp / B >= r->nblk) { /* next block still holds retained records */
		err = drop_oldest(r);
		if (err) {
			r->tp = old_tp;
			r->tail_seq = old_tail;
			return err;
		}
		moved = 1;
		if (r->tail_seq == r->next_seq) {
			r->tp = start;
		}
	}
	if (moved && (err = meta_commit(r))) { /* tail moves on flash before anything is erased */
		r->tp = old_tp;
		r->tail_seq = old_tail;
		return err;
	}
	k0 = start % B ? start / B + 1 : start / B;
	for (uint64_t k = k0 > r->blk_ready ? k0 : r->blk_ready; k <= k1; k++) {
		err = d->erase(d->ctx, 2 * B + (uint32_t)(k % r->nblk) * B);
		if (err) {
			return err;
		}
	}
	if (k1 + 1 > r->blk_ready) {
		r->blk_ready = k1 + 1;
	}
	h->seq = r->next_seq;
	h->boot = r->boot;
	chunk_hdr_encode(hb, h);
	err = put_stream(d, phys(r, start), hb, sizeof(hb), payload, h->len);
	if (!err) {
		r->wp = start + s;
		r->next_seq++;
	}
	return err;
}

int ring_ack(struct ring *r, uint32_t upto)
{
	if (upto <= r->acked || upto >= r->next_seq) {
		return -1;
	}
	r->acked = upto;
	return 0;
}

int ring_reclaim(struct ring *r)
{
	uint64_t old_tp = r->tp;
	uint32_t old_tail = r->tail_seq;
	int err, moved = 0;

	while (r->tail_seq <= r->acked && r->tail_seq < r->next_seq) {
		err = drop_oldest(r);
		if (err) {
			r->tp = old_tp;
			r->tail_seq = old_tail;
			return err;
		}
		moved = 1;
	}
	if (!moved) {
		return 0;
	}
	err = meta_commit(r);
	if (err) {
		r->tp = old_tp;
		r->tail_seq = old_tail;
	}
	return err;
}

int ring_pre_erase(struct ring *r, uint32_t n)
{
	const struct ring_dev *d = r->d;

	for (int err; n && r->blk_ready - r->tp / d->block < r->nblk; n--) {
		err = d->erase(d->ctx, 2 * d->block + (uint32_t)(r->blk_ready % r->nblk) * d->block);
		if (err) {
			return err;
		}
		r->blk_ready++;
	}
	return 0;
}

uint64_t ring_free(const struct ring *r)
{
	uint64_t B = r->d->block, used = 0;

	if (r->tail_seq != r->next_seq) {
		used = (r->wp - 1) / B - r->tp / B + 1;
	}
	return (r->nblk - used) * B;
}

uint64_t ring_erased_ahead(const struct ring *r)
{
	uint64_t end = r->blk_ready * r->d->block;

	return end > r->wp ? end - r->wp : 0;
}

int ring_gap(const struct ring *r, uint32_t *first, uint32_t *last)
{
	if (r->tail_seq <= r->acked + 1) {
		return 0;
	}
	*first = r->acked + 1;
	*last = r->tail_seq - 1;
	return 1;
}

int ring_find(const struct ring *r, const struct ring_rec *prev, struct ring_rec *out)
{
	uint64_t x;
	uint32_t len;
	int err;

	if (!prev) {
		if (r->tail_seq == r->next_seq) {
			return 1;
		}
		err = rec_at(r, r->tp, r->tail_seq, &len);
		if (err) {
			return err == -EIO ? -EIO : 1;
		}
		x = r->tp;
		out->seq = r->tail_seq;
	} else {
		if (prev->seq + 1 >= r->next_seq) {
			return 1;
		}
		err = probe(r, prev->pos + rsize(r, prev->len), prev->seq + 1, r->wp, &x, &len);
		if (err) {
			return err == -EIO ? -EIO : 1;
		}
		out->seq = prev->seq + 1;
	}
	out->pos = x;
	out->len = len;
	return 0;
}

int ring_read(const struct ring *r, const struct ring_rec *rec, uint32_t off, void *buf, size_t n)
{
	return r->d->read(r->d->ctx, phys(r, rec->pos) + off, buf, n);
}
