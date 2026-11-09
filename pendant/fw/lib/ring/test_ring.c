/* Host test: cc -std=c99 -O2 -Wall -Wextra -Werror -I../chunk -o /tmp/t test_ring.c ring.c ../chunk/chunk.c && /tmp/t
 * A flash simulator (program = AND only, erase per block, optional single program per page)
 * with power cuts injected at an exact byte of any program or erase, then recovery with ring_open(). */
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ring.h"

/* On failure, say exactly where: the runs are deterministic, so geometry + seed + operation
 * number + cut tick reproduce it (issue #13). `op` counts ring operations in a fuzz run;
 * `tick` is the byte at which the power was cut in a sweep. */
static struct {
	const char *geo, *phase;
	unsigned long long seed;
	long op, tick;
} ctx;

static void fail(const char *expr, const char *file, int line)
{
	fprintf(stderr, "FAILED %s (%s:%d)\n  geometry '%s', %s, seed 0x%llx, operation #%ld, cut tick %ld\n", expr, file,
		line, ctx.geo ? ctx.geo : "-", ctx.phase ? ctx.phase : "-", ctx.seed, ctx.op, ctx.tick);
	abort();
}
static void set_ctx(const char *geo, const char *phase, unsigned long long seed)
{
	ctx.geo = geo;
	ctx.phase = phase;
	ctx.seed = seed;
	ctx.op = 0;
	ctx.tick = -1;
}
#define assert(c) ((c) ? (void)0 : fail(#c, __FILE__, __LINE__))

/* ---------------- flash simulator ---------------- */

struct sim {
	uint8_t *mem, *progd;
	uint32_t size, block, page, wunit;
	int single, dead;
	long ticks; /* bytes left before the cut; <0 = no cut armed */
	long read_fail, read_skip, write_fail;
	uint32_t last_fail_addr;
	uint64_t rng;
	long violations, cut_write, cut_erase;
};

static uint32_t rnd(uint64_t *s)
{
	*s ^= *s << 13;
	*s ^= *s >> 7;
	*s ^= *s << 17;
	return (uint32_t)(*s >> 11);
}

static int sim_read(void *c, uint32_t a, void *buf, size_t n)
{
	struct sim *s = c;

	if (s->read_skip > 0) {
		s->read_skip--;
	} else if (s->read_fail > 0) {
		s->read_fail--;
		return -EIO;
	}
	if (a + n > s->size) {
		return -EINVAL;
	}
	memcpy(buf, s->mem + a, n);
	return 0;
}

static int sim_write(void *c, uint32_t a, const void *buf, size_t n)
{
	struct sim *s = c;
	const uint8_t *b = buf;

	if (s->dead || s->write_fail > 0) {
		if (!s->dead) {
			s->write_fail--;
			s->last_fail_addr = a;
		}
		return -EIO;
	}
	if (a + n > s->size || a % s->wunit || n % s->wunit ||
	    (s->single && (a % s->page || n % s->page))) {
		s->violations++;
		return -EINVAL;
	}
	for (size_t i = 0; i < n; i++) {
		uint32_t p = (a + i) / s->page;
		uint8_t o = s->mem[a + i];

		if (s->single && s->progd[p] == 1 && (i == 0 || (a + i) % s->page == 0)) {
			s->violations++; /* page programmed twice without erase */
		}
		if (s->ticks == 0) { /* power lost inside this byte: some bits made it */
			uint8_t r8 = rnd(&s->rng);

			if (!(b[i] & ~o)) {
				s->mem[a + i] = (o & r8) | (b[i] & ~r8);
			}
			for (size_t j = i; j < n; j += s->page) {
				s->progd[(a + j) / s->page] = 1;
			}
			s->dead = 1;
			s->cut_write++;
			return -EIO;
		}
		if (b[i] & ~o) {
			s->violations++; /* 0 -> 1 without erase */
		}
		s->mem[a + i] = o & b[i];
		if (s->ticks > 0) {
			s->ticks--;
		}
	}
	for (size_t j = 0; j < n; j += s->page) {
		s->progd[(a + j) / s->page] = 1;
	}
	return 0;
}

static int sim_erase(void *c, uint32_t a)
{
	struct sim *s = c;

	if (s->dead) {
		return -EIO;
	}
	if (a % s->block || a + s->block > s->size) {
		s->violations++;
		return -EINVAL;
	}
	for (uint32_t p = 0; p < s->block; p += s->page) {
		if (s->ticks >= 0 && s->ticks < (long)s->page) { /* cut: this page is half erased */
			for (uint32_t i = 0; i < s->page; i++) {
				s->mem[a + p + i] |= rnd(&s->rng);
			}
			s->progd[(a + p) / s->page] = 0;
			s->dead = 1;
			s->cut_erase++;
			return -EIO;
		}
		memset(s->mem + a + p, 0xff, s->page);
		s->progd[(a + p) / s->page] = 0;
		if (s->ticks > 0) {
			s->ticks -= s->page;
		}
	}
	return 0;
}

struct geo {
	const char *name;
	uint32_t block, page, wunit, nblocks, maxpay;
	int single;
	long cuts;
	int big;
};

struct fz {
	struct sim s;
	struct ring_dev d;
	struct ring r;
	const struct geo *g;
	uint8_t *pbuf, *buf, *pay;
	uint32_t maxrec, last, tail_low, *tlen;
	uint8_t *ttype;
	uint64_t rng;
	long cuts, reopens, drops_seen, appends;
	int acked_any;
};

static void fz_init(struct fz *f, const struct geo *g, uint64_t seed)
{
	memset(f, 0, sizeof(*f));
	f->g = g;
	f->rng = f->s.rng = seed;
	f->s.size = g->block * g->nblocks;
	f->s.block = g->block;
	f->s.page = g->page;
	f->s.wunit = g->wunit;
	f->s.single = g->single;
	f->s.ticks = -1;
	f->s.mem = malloc(f->s.size);
	f->s.progd = calloc(f->s.size / g->page, 1);
	assert(f->s.mem && f->s.progd);
	memset(f->s.mem, 0xff, f->s.size);
	f->pbuf = malloc(g->page);
	f->maxrec = chunk_size(g->maxpay);
	f->buf = malloc(f->maxrec + g->page);
	f->pay = malloc(g->maxpay);
	f->tlen = calloc(1 << 20, sizeof(uint32_t));
	f->ttype = calloc(1 << 20, 1);
	f->d = (struct ring_dev){.ctx = &f->s, .read = sim_read, .write = sim_write, .erase = sim_erase,
				 .size = f->s.size, .block = g->block, .page = g->page,
				 .wunit = g->wunit, .single_program = g->single, .pbuf = f->pbuf};
}

static void fz_free(struct fz *f)
{
	free(f->s.mem);
	free(f->s.progd);
	free(f->pbuf);
	free(f->buf);
	free(f->pay);
	free(f->tlen);
	free(f->ttype);
}

static void gen(uint32_t seq, uint32_t len, uint8_t *o)
{
	for (uint32_t i = 0; i < len; i++) {
		o[i] = (uint8_t)((seq * 2654435761u >> 11) ^ (i * 131u));
	}
}

/* Everything the ring says it holds is intact and nothing committed is missing. */
static void check(struct fz *f, int full)
{
	struct ring *r = &f->r;
	struct ring_rec rec, prev;
	int have = 0;
	uint32_t expect = r->tail_seq, n = 0;
	uint64_t retained = 0;

	while (!ring_find(r, have ? &prev : NULL, &rec)) {
		assert(rec.seq == expect);
		assert(f->tlen[expect] == rec.len);
		retained += (chunk_size(rec.len) + f->g->page - 1) / f->g->page * f->g->page;
		if (full || n < 4 || (rnd(&f->rng) % 4096) == 0) {
			struct chunk_hdr h;
			const uint8_t *p;

			assert(!ring_read(r, &rec, 0, f->buf, chunk_size(rec.len)));
			assert(chunk_decode(f->buf, chunk_size(rec.len), &h, &p) == CHUNK_OK);
			assert(h.seq == rec.seq && h.type == f->ttype[rec.seq] && h.len == rec.len);
			gen(rec.seq, rec.len, f->pay);
			assert(!memcmp(p, f->pay, rec.len));
		}
		prev = rec;
		have = 1;
		expect++;
		n++;
	}
	assert(expect == r->next_seq);
	assert(r->next_seq - 1 >= f->last && r->next_seq - 1 <= f->last + 1);
	assert(r->tail_seq >= f->tail_low);
	if (!f->acked_any && r->tail_seq > 1) { /* overwrite drops only what it must */
		uint64_t R = (uint64_t)r->nblk * f->g->block;

		/* Free space can be: dead bytes before the tail, the wrap hole, the overshoot of the last
		 * dropped record, the record whose drop was cut before it was written, and a skipped
		 * block remainder after a crash: 3 records and 2 blocks at most. */
		assert(retained + 3 * (uint64_t)r->maxrec + 2 * f->g->block >= R);
	}
	f->last = r->next_seq - 1;
	f->tail_low = r->tail_seq;
}

/* Revive after a cut or a clean reboot. The open itself may be cut too. */
static void reopen(struct fz *f)
{
	for (;;) {
		int err;

		f->s.dead = 0;
		err = ring_open(&f->r, &f->d, f->maxrec);
		if (!err) {
			break;
		}
		assert(err == -EIO && f->s.dead);
		f->cuts++;
		f->s.ticks = -1;
	}
	f->s.ticks = -1;
	check(f, !f->g->big);
}

static int do_append(struct fz *f, uint32_t len, uint8_t type)
{
	struct chunk_hdr h = {.type = type, .codec = 1, .mono_ms = rnd(&f->rng), .rtc_ms = rnd(&f->rng), .len = len};
	uint32_t seq = f->r.next_seq;

	f->tlen[seq] = len;
	f->ttype[seq] = type;
	gen(seq, len, f->pay);
	f->appends++;
	return ring_append(&f->r, &h, f->pay);
}

static uint32_t pick_len(struct fz *f, uint8_t *type)
{
	uint32_t k = rnd(&f->rng) % 10;

	if (k < 1) {
		*type = CHUNK_TIMEFIX;
		return 16;
	}
	if (k < 2) {
		*type = CHUNK_GAP;
		return 12;
	}
	*type = CHUNK_AUDIO;
	return k < 6 ? f->g->maxpay : (1 + rnd(&f->rng) % (f->g->maxpay / 4)) * 4;
}

/* One random operation, with a power cut armed with probability 1/cutp. Returns 1 if cut. */
static int step(struct fz *f, int allow_ack, unsigned cutp)
{
	uint32_t k = rnd(&f->rng) % 100, before = f->r.tail_seq;
	int err = 0;

	ctx.op++;

	if (cutp && rnd(&f->rng) % cutp == 0) {
		f->s.ticks = rnd(&f->rng) % (2 * f->maxrec + 2 * f->g->block);
	}
	if (k < 70 || !allow_ack) {
		uint8_t t;
		uint32_t len = pick_len(f, &t);

		err = do_append(f, len, t);
	} else if (k < 85) {
		uint32_t span = f->r.next_seq - 1 - f->r.acked;

		if (span) {
			ring_ack(&f->r, f->r.acked + 1 + rnd(&f->rng) % span);
		}
		f->acked_any = 1;
		err = ring_reclaim(&f->r);
	} else if (k < 93) {
		err = ring_pre_erase(&f->r, 1 + rnd(&f->rng) % 4);
	} else {
		f->s.ticks = -1;
		reopen(f);
		f->reopens++;
		return 0;
	}
	if (f->r.tail_seq > before && !f->s.dead) {
		f->drops_seen++;
	}
	if (err == -EIO && f->s.dead) {
		f->cuts++;
		f->s.ticks = -1;
		reopen(f);
		return 1;
	}
	assert(!err);
	f->s.ticks = -1;
	if (f->r.tail_seq > before) {
		f->tail_low = f->r.tail_seq;
	}
	f->last = f->r.next_seq - 1;
	return 0;
}

static void fuzz(const struct geo *g, uint64_t seed)
{
	struct fz f;
	uint64_t R;

	set_ctx(g->name, "random cuts", seed);
	fz_init(&f, g, seed);
	assert(!ring_format(&f.r, &f.d, f.maxrec));
	f.last = 0;
	f.tail_low = 1;
	reopen(&f);
	R = (uint64_t)f.r.nblk * g->block;

	/* phase A: never acked, 2.3 laps: the ring must overwrite the oldest and stay valid */
	while (f.appends * (uint64_t)(g->maxpay / 2) < R * 23 / 10 || f.cuts < g->cuts / 2) {
		step(&f, 0, g->big ? 4000 : 8);
	}
	{
		uint32_t first, last;

		assert(ring_gap(&f.r, &first, &last) && first == 1 && last == f.r.tail_seq - 1);
	}
	/* phase B: acks, reclaims, pre-erase, reboots */
	while (f.cuts < g->cuts) {
		step(&f, 1, g->big ? 3000 : 8);
	}
	printf("  %-34s cuts %5ld (write %ld, erase %ld)  reopens %4ld  appends %6ld  laps %.1f  violations %ld\n",
	       g->name, f.cuts, f.s.cut_write, f.s.cut_erase, f.reopens, f.appends, (double)f.r.wp / R,
	       f.s.violations);
	assert(f.s.violations == 0);
	fz_free(&f);
}

/* Cut at every byte tick (every ~50th for big records) of one operation, from several ring
 * states: fresh, mid-block, full and wrapping, after reclaims. Each cut is recovered and checked,
 * then the medium is put back so the next tick starts from the same state. */
static void sweep(const struct geo *g, uint64_t seed)
{
	struct fz f;
	uint8_t *mem0, *prog0;
	long runs = 0;
	int small = g->maxpay <= 3000;

	set_ctx(g->name, "one operation cut at every tick", seed);
	fz_init(&f, g, seed);
	mem0 = malloc(f.s.size);
	prog0 = malloc(f.s.size / g->page);
	assert(!ring_format(&f.r, &f.d, f.maxrec));
	reopen(&f);
	for (int state = 0; state < 10; state++) {
		for (int i = 0; i < 1 + state * 7; i++) {
			uint8_t t;
			uint32_t len = pick_len(&f, &t);

			assert(!do_append(&f, len, t));
			f.last = f.r.next_seq - 1;
		}
		if (state % 3 == 2) {
			ring_ack(&f.r, f.r.next_seq - 1 - f.r.next_seq / 4);
			assert(!ring_reclaim(&f.r));
			f.tail_low = f.r.tail_seq;
		}
		f.acked_any = state % 3 == 2 || f.acked_any;
		memcpy(mem0, f.s.mem, f.s.size);
		memcpy(prog0, f.s.progd, f.s.size / g->page);
		for (int op = 0; op < 3; op++) {
			uint32_t len = op == 0 ? g->maxpay : 16u + 4u * (unsigned)(state % 5);
			struct fz saved = f;

			saved.acked_any = op == 2 || f.acked_any;
			f = saved;
			saved = f;
			for (long t = 0;; t += g->wunit == 1 ? 1 : 1 + rnd(&f.rng) % (small ? 3 : 97)) {
				uint64_t rng = f.rng * 6364136223846793005ULL + 1442695040888963407ULL;
				int err;

				f.s.ticks = ctx.tick = t;
				ctx.op = state * 10 + op;
				f.s.dead = 0;
				if (op == 2) {
					ring_ack(&f.r, f.r.next_seq - 1);
					err = ring_reclaim(&f.r);
				} else {
					err = do_append(&f, len, op == 0 ? CHUNK_AUDIO : CHUNK_TIMEFIX);
				}
				assert(!err || (err == -EIO && f.s.dead));
				f.s.ticks = -1;
				f.last = err ? f.last : f.r.next_seq - 1;
				reopen(&f);
				runs++;
				memcpy(f.s.mem, mem0, f.s.size);
				memcpy(f.s.progd, prog0, f.s.size / g->page);
				saved.s.violations = f.s.violations;
				saved.s.cut_write = f.s.cut_write;
				saved.s.cut_erase = f.s.cut_erase;
				saved.rng = saved.s.rng = rng;
				f = saved;
				if (!err) {
					break;
				}
			}
		}
	}
	printf("  %-34s cut points %6ld (write %ld, erase %ld)  violations %ld\n", g->name, runs,
	       f.s.cut_write, f.s.cut_erase, f.s.violations);
	assert(f.s.violations == 0);
	free(mem0);
	free(prog0);
	fz_free(&f);
}

static void test_io_errors(void)
{
	static const struct geo g = {"read/write fault", 1024, 64, 1, 34, 1500, 0, 1500, 0};
	struct fz f;
	struct ring_rec rec;

	set_ctx(g.name, "I/O errors", 1);
	fz_init(&f, &g, 1);
	assert(!ring_format(&f.r, &f.d, f.maxrec));
	f.last = 0;
	f.tail_low = 1;
	reopen(&f);
	for (int i = 0; i < 5; i++) {
		assert(!do_append(&f, 100, CHUNK_AUDIO));
	}
	f.last = f.r.next_seq - 1;

	/* One transient metadata read error must not look like an unformatted ring. */
	f.s.read_fail = 2;
	assert(ring_open(&f.r, &f.d, f.maxrec) == -EIO);
	f.s.read_fail = 0;
	assert(!ring_open(&f.r, &f.d, f.maxrec));
	check(&f, 1);

	/* A read error during the tail walk must abort the open, not truncate it. */
	f.s.read_skip = 2; /* two metadata reads succeed */
	f.s.read_fail = 1; /* the first record read fails */
	assert(ring_open(&f.r, &f.d, f.maxrec) == -EIO);
	f.s.read_fail = 0;
	f.s.read_skip = 0;
	assert(!ring_open(&f.r, &f.d, f.maxrec));
	check(&f, 1);

	/* A failed metadata commit after dropping the oldest must roll the RAM tail back. */
	while (ring_free(&f.r) > 0) {
		assert(!do_append(&f, g.maxpay, CHUNK_AUDIO));
	}
	assert(f.r.tail_seq > 1);
	uint32_t tail = f.r.tail_seq, next = f.r.next_seq;
	f.s.last_fail_addr = UINT32_MAX;
	f.s.write_fail = 1;
	assert(do_append(&f, g.maxpay, CHUNK_AUDIO) == -EIO);
	f.s.write_fail = 0;
	assert(f.s.last_fail_addr < 2 * g.block); /* the metadata write, before any erase */
	assert(f.r.tail_seq == tail);
	assert(!ring_find(&f.r, NULL, &rec) && rec.seq == tail); /* the old tail is still readable */
	assert(!ring_open(&f.r, &f.d, f.maxrec));
	assert(f.r.tail_seq == tail && f.r.next_seq == next);
	assert(!do_append(&f, g.maxpay, CHUNK_AUDIO));
	f.last = f.r.next_seq - 1;
	check(&f, 1);

	/* A failed metadata commit in reclaim must not advance the acked tail. */
	assert(!ring_ack(&f.r, f.r.next_seq - 1));
	f.acked_any = 1;
	tail = f.r.tail_seq;
	f.s.write_fail = 1;
	assert(ring_reclaim(&f.r) == -EIO);
	f.s.write_fail = 0;
	assert(f.r.tail_seq == tail);
	assert(!ring_reclaim(&f.r));
	f.tail_low = f.r.tail_seq;
	check(&f, 1);
	/* A ring built with a different record geometry must not be reformatted/truncated. */
	{
		struct ring other;
		uint32_t larger = f.maxrec + g.page;

		assert(ring_open(&other, &f.d, larger) == -EPROTO);
		assert(!ring_open(&other, &f.d, f.maxrec));
	}
	fz_free(&f);
	puts("  read/write fault: no format, no truncation, no durable-tail drift; geometry mismatch fails");
}

static void test_v1_metadata(void)
{
	enum { BLK = 1024, NBLK = 8 };
	struct sim sim = {.size = BLK * NBLK, .block = BLK, .page = 64, .wunit = 1};
	struct ring_dev d;
	struct ring r;
	uint8_t pbuf[64], m[36] = {0};

	sim.mem = malloc(sim.size);
	sim.progd = calloc(sim.size / sim.page, 1);
	assert(sim.mem && sim.progd);
	memset(sim.mem, 0xff, sim.size);
	memcpy(m, "LOSM", 4);
	m[4] = 1; /* gen */
	m[16] = 1; /* tail_seq */
	m[24] = 1; /* boot */
	{
		uint32_t crc = chunk_crc32(0, m, 28);
		uint8_t *p = m + 28;

		for (int i = 0; i < 4; i++) {
			p[i] = (uint8_t)(crc >> (8 * i));
		}
		memcpy(m + 32, chunk_commit_word, 4);
	}
	memcpy(sim.mem, m, sizeof(m));
	d = (struct ring_dev){.ctx = &sim, .read = sim_read, .write = sim_write, .erase = sim_erase,
			      .size = sim.size, .block = sim.block, .page = sim.page, .wunit = sim.wunit,
			      .pbuf = pbuf};
	assert(ring_open(&r, &d, 1024) == -EPROTO);
	assert(sim.mem[0] == 'L'); /* not erased */
	free(sim.mem);
	free(sim.progd);
}

static void test_sim_rules(void)
{
	struct sim s = {.size = 4096, .block = 1024, .page = 64, .wunit = 1};
	uint8_t z = 0x00, ff = 0xff, v;

	s.mem = malloc(4096);
	s.progd = calloc(64, 1);
	s.ticks = -1;
	memset(s.mem, 0xff, 4096);
	assert(!sim_write(&s, 5, &z, 1) && s.violations == 0);
	assert(!sim_write(&s, 5, &ff, 1) && s.violations == 1); /* 0 -> 1 refused, byte stays 0 */
	sim_read(&s, 5, &v, 1);
	assert(v == 0x00);
	assert(!sim_erase(&s, 0));
	sim_read(&s, 5, &v, 1);
	assert(v == 0xff);
	s.single = 1;
	memset(s.mem, 0xff, 4096);
	{
		uint8_t pg[64];

		memset(pg, 0xaa, 64);
		assert(!sim_write(&s, 0, pg, 64) && s.violations == 1);
		assert(!sim_write(&s, 0, pg, 64) && s.violations == 2); /* second program of a page */
	}
	free(s.mem);
	free(s.progd);
	puts("  simulator: 0->1 and double page program are flagged");
}

/* ACK rules and gap reporting, deterministic. */
static void test_ack_gap(void)
{
	static const struct geo g = {"ack/gap", 1024, 64, 1, 34, 1500, 0, 0, 0};
	set_ctx(g.name, "ack and gap rules", 7);
	struct fz f;
	struct ring_rec rec;
	uint32_t first, last;

	fz_init(&f, &g, 7);
	assert(!ring_format(&f.r, &f.d, f.maxrec) && !ring_open(&f.r, &f.d, f.maxrec));
	assert(ring_find(&f.r, NULL, &rec) == 1); /* empty */
	for (int i = 0; i < 5; i++) {
		assert(!do_append(&f, 100, CHUNK_AUDIO));
	}
	assert(f.r.next_seq == 6 && ring_ack(&f.r, 9) == -1 && f.r.acked == 0); /* beyond newest */
	assert(ring_ack(&f.r, 6) == -1);
	assert(!ring_ack(&f.r, 3) && ring_ack(&f.r, 2) == -1 && ring_ack(&f.r, 3) == -1); /* backwards, repeat */
	assert(!ring_gap(&f.r, &first, &last));
	assert(!ring_reclaim(&f.r) && f.r.tail_seq == 4);
	assert(!ring_find(&f.r, NULL, &rec) && rec.seq == 4);
	assert(!ring_find(&f.r, &rec, &rec) && rec.seq == 5);
	assert(ring_find(&f.r, &rec, &rec) == 1);
	f.last = 5;
	f.tail_low = 4;
	f.acked_any = 1;
	assert(!ring_open(&f.r, &f.d, f.maxrec) && f.r.tail_seq == 4 && f.r.acked == 3); /* persisted */
	assert(!ring_ack(&f.r, 5) && !ring_reclaim(&f.r) && f.r.tail_seq == f.r.next_seq);
	assert(ring_find(&f.r, NULL, &rec) == 1); /* empty again, next_seq keeps counting */
	assert(!do_append(&f, 100, CHUNK_AUDIO) && f.r.next_seq == 7);
	/* never acked: overwrite reports exactly the dropped range, and it survives a reboot */
	while (f.r.tail_seq == 6) {
		assert(!do_append(&f, 1500, CHUNK_AUDIO));
	}
	assert(ring_gap(&f.r, &first, &last) == 0 || first == 6); /* acked is 5, so no gap before 6 */
	while (f.r.tail_seq < 40) {
		assert(!do_append(&f, 1500, CHUNK_AUDIO));
	}
	assert(ring_gap(&f.r, &first, &last) && first == 6 && last == f.r.tail_seq - 1);
	{
		uint32_t t = f.r.tail_seq;

		f.last = f.r.next_seq - 1;
		f.tail_low = t;
		assert(!ring_open(&f.r, &f.d, f.maxrec) && f.r.tail_seq == t);
		assert(ring_gap(&f.r, &first, &last) && first == 6 && last == t - 1);
		assert(!ring_ack(&f.r, t - 1 + 1) && !ring_gap(&f.r, &first, &last)); /* phone took the gap */
	}
	assert(f.s.violations == 0);
	fz_free(&f);
	puts("  ack rules, empty ring, gap range survives a reboot");
}

int main(int argc, char **argv)
{
	static const struct geo geos[] = {
		{"NOR  1K/64B", 1024, 64, 1, 34, 1500, 0, 1500, 0},
		{"NAND 2K/256B single-program", 2048, 256, 256, 28, 3000, 1, 1500, 0},
		{"RRAM 4K/256B/16B (Phase 0, 712 KB)", 4096, 256, 16, 178, 32000, 0, 400, 0},
		{"Premium NAND 256K/4K (516 MB)", 262144, 4096, 4096, 2066, 32728, 1, 12, 1},
	};

	test_sim_rules();
	test_v1_metadata();
	test_io_errors();
	test_ack_gap();
	puts("random power cuts, then recovery:");
	(void)argv;
	int quick = argc > 1; /* ./t quick: first geometry only, for mutation checks */

	for (unsigned i = 0; i < (quick ? 1 : sizeof(geos) / sizeof(geos[0])); i++) {
		fuzz(&geos[i], 0x9e3779b97f4a7c15ULL + i);
	}
	puts("one operation, cut at every byte (NOR) or every few bytes:");
	for (unsigned i = 0; i < (quick ? 1 : 3); i++) {
		sweep(&geos[i], 0x1234567ULL + i);
	}
	puts("ring: all tests passed");
	return 0;
}
