#include <errno.h>
#include <string.h>
#include "sync.h"

#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((a) > (b) ? (a) : (b))

static void wr32(uint8_t *p, uint32_t v)
{
	for (int i = 0; i < 4; i++) {
		p[i] = (uint8_t)(v >> (8 * i));
	}
}

void sync_init(struct sync *s, struct ring *r, const struct sync_io *io)
{
	memset(s, 0, sizeof(*s));
	s->ring = r;
	s->io = io;
}

/* One record, or the part of it from off on, as fragments. */
static int send_record(struct sync *s, uint32_t seq, const uint8_t *rec, uint32_t size, uint32_t off)
{
	uint8_t pkt[8 + 512];
	uint32_t frag = MIN(s->io->frag, (uint32_t)sizeof(pkt) - 8);

	for (; off < size; off += frag) {
		uint32_t n = MIN(frag, size - off);
		int err;

		wr32(pkt, seq);
		wr32(pkt + 4, off);
		memcpy(pkt + 8, rec + off, n);
		err = s->io->tx(s->io->ctx, pkt, 8 + n);
		if (err) {
			return err;
		}
		s->io->st->bytes_sent += n;
	}
	return 0;
}

/* Loads record `seq` into the scratch buffer under the lock. Returns its size, 0 if it is gone. */
/* 0 = found and copied, 1 = no longer in the ring, -EIO = device read error,
 * -EMSGSIZE = record larger than the sync buffer. */
static int load(struct sync *s, uint32_t seq, uint32_t *size)
{
	const struct sync_io *io = s->io;
	struct ring_rec rec, prev;
	int err;

	io->lock(io->ctx);
	err = ring_find(s->ring, NULL, &rec);
	while (err == 0 && rec.seq < seq) {
		prev = rec;
		err = ring_find(s->ring, &prev, &rec);
	}
	if (err < 0) {
		io->unlock(io->ctx);
		return err;
	}
	if (err == 0 && rec.seq == seq) {
		size_t n = chunk_size(rec.len);

		if (n > io->buf_size) {
			io->unlock(io->ctx);
			return -EMSGSIZE;
		}
		if (ring_read(s->ring, &rec, 0, io->buf, n)) {
			io->unlock(io->ctx);
			return -EIO;
		}
		*size = (uint32_t)n;
		io->unlock(io->ctx);
		return 0;
	}
	io->unlock(io->ctx);
	return 1;
}

/* Tell the phone that records [first, last] are gone (overwritten): a gap record, docs/spec/formats.md §5. */
static int send_gap(struct sync *s, uint32_t first, uint32_t last)
{
	uint8_t gp[CHUNK_GAP_SIZE], rec[CHUNK_HDR_SIZE + CHUNK_GAP_SIZE + CHUNK_TRAILER_SIZE];
	uint64_t rtc = s->io->rtc_ms(s->io->ctx);
	struct chunk_hdr h = {.type = CHUNK_GAP, .seq = first, .boot = s->ring->boot,
			      .mono_ms = s->io->mono_ms(s->io->ctx), .rtc_ms = rtc,
			      .flags = rtc ? 0 : CHUNK_F_RTC_INVALID, .len = CHUNK_GAP_SIZE};

	chunk_gap_encode(gp, first, last, CHUNK_GAP_OVERWRITTEN);
	chunk_encode(rec, &h, gp, 1);
	return send_record(s, first, rec, sizeof(rec), 0);
}

static uint32_t ring_tail(struct sync *s)
{
	uint32_t t;

	s->io->lock(s->io->ctx);
	t = s->ring->tail_seq;
	s->io->unlock(s->io->ctx);
	return t;
}

void sync_drain(struct sync *s)
{
	const struct sync_io *io = s->io;
	struct sync_cmd c;

	while (io->poll(io->ctx, &c)) {
		switch (c.op) {
		case SYNC_OP_ACK:
			io->lock(io->ctx);
			if (!ring_ack(s->ring, c.a)) {
				io->st->acks++;
			}
			io->unlock(io->ctx);
			break;
		case SYNC_OP_TIME:
			if (io->other) {
				io->other(io->ctx, &c);
			}
			break;
		case SYNC_OP_SYNC:
			s->have = c.a;
			s->restart = 1;
			s->abort = 0; /* this SYNC is newer than any earlier STOP */
			break;
		case SYNC_OP_STOP:
			s->abort = 1;
			break;
		case SYNC_OP_RESEND: {
			int i = 0;

			while (i < s->nrs && s->rs[i].seq != c.a) {
				i++;
			}
			if (i == s->nrs) {
				if (s->nrs == 8) {
					break; /* more outstanding than we track: the phone will ask again */
				}
				s->nrs++;
				s->rs[i].off = c.b;
			} else {
				s->rs[i].off = MIN(s->rs[i].off, c.b);
			}
			s->rs[i].seq = c.a;
			break;
		}
		case SYNC_OP_RECLAIM:
			io->lock(io->ctx);
			if (ring_reclaim(s->ring)) {
				s->abort = 1;
			}
			io->unlock(io->ctx);
			break;
		}
	}
}

int sync_resend(struct sync *s, uint32_t seq, uint32_t off)
{
	uint32_t size = 0;
	int err;

	if (seq == 0 || seq >= s->ring->next_seq) {
		return 1; /* 0 is the end-marker sentinel; anything newer is not retained */
	}
	err = load(s, seq, &size);
	s->io->st->resends++;
	if (err < 0) {
		return -1;
	}
	if (!err) {
		return send_record(s, seq, s->io->buf, size, off);
	}
	{
		uint32_t t = ring_tail(s);

		if (t > seq) {
			return send_gap(s, seq, t - 1); /* gone since: a gap tells the phone to move on */
		}
	}
	return 1; /* nothing to send */
}

int sync_run(struct sync *s, uint32_t have)
{
	struct ring *r = s->ring;
	const struct sync_io *io = s->io;
	uint32_t tail, next;

	if (have == UINT32_MAX) {
		return -1; /* would make have+1 wrap to the seq=0 end marker */
	}
	s->have = have;
restart:
	s->restart = 0;
	io->st->syncs++;
	io->lock(io->ctx);
	ring_ack(r, s->have);
	tail = r->tail_seq;
	next = r->next_seq;
	io->unlock(io->ctx);
	if (tail > s->have + 1 && send_gap(s, s->have + 1, tail - 1)) { /* the phone missed records that were overwritten */
		return -1;
	}
	for (uint32_t seq = MAX(tail, s->have + 1); seq < next && !s->restart && !s->abort; seq++) {
		uint32_t size = 0;
		int lr = load(s, seq, &size);

		if (lr < 0) {
			return -1;
		}
		if (seq + 1 == next) { /* records appended while we send belong to this sync too */
			io->lock(io->ctx);
			next = r->next_seq;
			io->unlock(io->ctx);
		}
		sync_drain(s);
		if (s->restart || s->abort) {
			break;
		}
		if (lr > 0) { /* overwritten while we were sending: say so, or the phone waits for it forever */
			uint32_t t = ring_tail(s);

			if (t > seq && send_gap(s, seq, t - 1)) {
				return -1;
			}
			seq = MAX(seq, t - 1);
			continue;
		}
		if (send_record(s, seq, io->buf, size, 0)) {
			return -1;
		}
		io->st->records_sent++;
		sync_drain(s);
		while (s->nrs && !s->restart && !s->abort) { /* the phone lost fragments of earlier records */
			uint32_t rseq = s->rs[0].seq, roff = s->rs[0].off;

			memmove(&s->rs[0], &s->rs[1], (size_t)(--s->nrs) * sizeof(s->rs[0]));
			if (sync_resend(s, rseq, roff) < 0) {
				return -1;
			}
		}
	}
	if (s->restart) {
		goto restart;
	}
	if (s->abort) {
		return 1;
	}
	{
		uint8_t end[8] = {0};

		io->lock(io->ctx);
		next = r->next_seq;
		io->unlock(io->ctx);
		wr32(end + 4, next);
		return io->tx(io->ctx, end, sizeof(end)) ? -1 : 0;
	}
}
