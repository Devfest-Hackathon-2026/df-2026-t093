/* Host test (issue #7): the pendant-side sender (sync.c) against a phone model (a port of tools/pendant_rx.py's
 * receiver: fragment reassembly, CRC check, in-order cumulative ACK, RESEND on a hole) over a link that drops,
 * duplicates, reorders and corrupts packets and disconnects at any packet, while the recorder keeps appending.
 * cc -std=c99 -O2 -Wall -Wextra -Werror -I../chunk -I../ring -o /tmp/t test_sync.c sync.c ../ring/ring.c ../chunk/chunk.c */
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "sync.h"

static struct {
	unsigned long long seed;
	const char *scn;
} ctx;

static void fail(const char *expr, const char *file, int line)
{
	fprintf(stderr, "FAILED %s (%s:%d)\n  scenario '%s', seed %llu\n", expr, file, line, ctx.scn, ctx.seed);
	abort();
}
#define assert(c) ((c) ? (void)0 : fail(#c, __FILE__, __LINE__))

#define BLOCK 4096u
#define PAGE 256u
#define MAXPAY 1000u
#define MAXREC ((uint32_t)(CHUNK_HDR_SIZE + MAXPAY + CHUNK_TRAILER_SIZE))
#define FRAG 244u
#define NSEQ 4096 /* sequence numbers a run can use */

static uint64_t rng_state;
static uint32_t rnd(void)
{
	rng_state ^= rng_state << 13;
	rng_state ^= rng_state >> 7;
	rng_state ^= rng_state << 17;
	return (uint32_t)(rng_state >> 11);
}
static int chance(unsigned per_mille)
{
	return rnd() % 1000 < per_mille;
}

/* ---------------- RAM flash ---------------- */

static uint8_t mem[80 * BLOCK];
static uint32_t mem_size;

static long read_fail_after = -1;
static int f_read(void *c, uint32_t a, void *b, size_t n)
{
	(void)c;
	if (read_fail_after >= 0 && read_fail_after-- == 0) {
		return -EIO;
	}
	assert(a + n <= mem_size);
	memcpy(b, mem + a, n);
	return 0;
}
static int f_write(void *c, uint32_t a, const void *b, size_t n)
{
	(void)c;
	assert(a + n <= mem_size);
	for (size_t i = 0; i < n; i++) {
		assert(!(((const uint8_t *)b)[i] & ~mem[a + i])); /* program only clears bits */
		mem[a + i] &= ((const uint8_t *)b)[i];
	}
	return 0;
}
static int f_erase(void *c, uint32_t a)
{
	(void)c;
	assert(a % BLOCK == 0 && a + BLOCK <= mem_size);
	memset(mem + a, 0xff, BLOCK);
	return 0;
}

/* ---------------- the pendant side ---------------- */

static struct ring ring;
static struct ring_dev dev;
static uint8_t pbuf[PAGE], recbuf[MAXREC];
static struct sync_stats st;
static long total_stops, total_gaps, total_rejected, total_resends;
static uint32_t appended; /* highest sequence number the recorder has written */
static uint32_t limit;    /* the recorder stops here */

static uint8_t pattern(uint32_t seq, uint32_t i)
{
	return (uint8_t)(seq * 31u + i * 7u + 5u);
}

static void append_one(void)
{
	static uint8_t pay[MAXPAY];
	struct chunk_hdr h = {.type = CHUNK_AUDIO, .codec = CHUNK_CODEC_IMA4_8K, .mono_ms = appended * 500u};
	uint32_t len = 100 + (rnd() % ((MAXPAY - 100) / 4)) * 4, seq = ring.next_seq;

	h.len = len;
	for (uint32_t i = 0; i < len; i++) {
		pay[i] = pattern(seq, i);
	}
	assert(!ring_append(&ring, &h, pay) && h.seq == seq);
	appended = seq;
}

/* ---------------- the phone side (port of tools/pendant_rx.py) ---------------- */

struct rx {
	uint32_t next;                /* next sequence expected; acked = next - 1 */
	struct { uint32_t seq, len; uint8_t buf[MAXREC]; int used; } part[8];
	struct { uint32_t seq, first, last, size; uint8_t type, rec[MAXREC]; int used; } ahead[512];
	struct { uint32_t seq, off; int used; } wait[8];
	int end_seen;
	uint32_t end_next;
	/* verification */
	uint8_t covered[NSEQ];        /* 1 = delivered as a record, 2 = covered by a gap record */
	long delivered, dups, rejected, resends_asked, acks;
};
static struct rx rx;

static struct sync_cmd cq[256];
static int cq_n;
static void push(uint8_t op, uint32_t a, uint32_t b)
{
	assert(cq_n < 256);
	cq[cq_n++] = (struct sync_cmd){.op = op, .a = a, .b = b};
}

static void ask_resend(uint32_t seq, uint32_t off)
{
	int i = 0;

	while (i < 8 && !(rx.wait[i].used && rx.wait[i].seq == seq)) {
		i++;
	}
	if (i == 8) {
		for (i = 0; i < 8 && rx.wait[i].used; i++) {
		}
		if (i == 8) {
			i = 0; /* table full: reuse, the end-marker retry covers what is lost */
		}
	}
	rx.wait[i].used = 1;
	rx.wait[i].seq = seq;
	rx.wait[i].off = off;
	rx.resends_asked++;
	push(SYNC_OP_RESEND, seq, off);
}

static int is_gap(const uint8_t *rec, uint32_t *first, uint32_t *last)
{
	struct chunk_hdr h;
	const uint8_t *p;

	assert(chunk_decode(rec, MAXREC, &h, &p) == CHUNK_OK);
	if (h.type != CHUNK_GAP) {
		return 0;
	}
	uint8_t why;

	assert(h.boot == ring.boot);
	assert(h.flags & CHUNK_F_RTC_INVALID); /* io_rtc() returns 0 in this harness */
	chunk_gap_decode(p, first, last, &why);
	return 1;
}

static void accept_in_order(void)
{
	for (;;) {
		int pick = -1;

		for (int i = 0; i < 512; i++) { /* lowest sequence that is not ahead of us */
			if (rx.ahead[i].used && rx.ahead[i].first <= rx.next && (pick < 0 || rx.ahead[i].seq < rx.ahead[pick].seq)) {
				pick = i;
			}
		}
		if (pick < 0) {
			return;
		}
		__typeof__(rx.ahead[0]) *a = &rx.ahead[pick];

		a->used = 0;
		if (a->last < rx.next) {
			rx.dups++; /* already have it */
			continue;
		}
		if (a->type == CHUNK_GAP) {
			for (uint32_t s = a->first < rx.next ? rx.next : a->first; s <= a->last; s++) { /* a stale RESEND can yield a gap that overlaps what we hold */
				assert(s < NSEQ && !rx.covered[s]); /* never both a record and a gap */
				rx.covered[s] = 2;
			}
		} else {
			struct chunk_hdr h;
			const uint8_t *p;

			assert(chunk_decode(a->rec, a->size, &h, &p) == CHUNK_OK); /* only CRC-checked records get here */
			for (uint32_t i = 0; i < h.len; i++) {
				assert(p[i] == pattern(h.seq, i)); /* and the bytes are the recorder's */
			}
			assert(a->seq < NSEQ && !rx.covered[a->seq]);
			rx.covered[a->seq] = 1;
			rx.delivered++;
		}
		rx.next = a->last + 1;
	}
}

static void rx_fragment(const uint8_t *d, size_t n)
{
	uint32_t seq = d[0] | d[1] << 8 | d[2] << 16 | (uint32_t)d[3] << 24, off = d[4] | d[5] << 8 | d[6] << 16 | (uint32_t)d[7] << 24;

	if (seq == 0) {
		rx.end_seen = 1;
		rx.end_next = off;
		return;
	}
	for (int i = 0; i < 8; i++) { /* a resend we asked for, or the old stream until it arrives */
		if (rx.wait[i].used && rx.wait[i].seq == seq) {
			if (off == rx.wait[i].off) {
				rx.wait[i].used = 0;
			} else if (off == 0) {
				rx.wait[i].used = 0;
				for (int j = 0; j < 8; j++) {
					if (rx.part[j].used && rx.part[j].seq == seq) {
						rx.part[j].used = 0;
					}
				}
			} else {
				return;
			}
		}
	}
	int pi = -1;

	for (int i = 0; i < 8; i++) {
		if (rx.part[i].used && rx.part[i].seq == seq) {
			pi = i;
		}
	}
	if (pi < 0) {
		for (int i = 0; i < 8 && pi < 0; i++) {
			if (!rx.part[i].used) {
				pi = i;
			}
		}
		if (pi < 0) {
			pi = 0; /* too many partial records: drop the oldest; the end-marker retry asks again */
		}
		rx.part[pi].used = 1;
		rx.part[pi].seq = seq;
		rx.part[pi].len = 0;
	}
	if (off != rx.part[pi].len) {
		ask_resend(seq, rx.part[pi].len); /* a hole or a repeat */
		return;
	}
	assert(rx.part[pi].len + n - 8 <= MAXREC);
	memcpy(rx.part[pi].buf + rx.part[pi].len, d + 8, n - 8);
	rx.part[pi].len += (uint32_t)n - 8;
	if (rx.part[pi].len < CHUNK_HDR_SIZE) {
		return;
	}
	uint32_t hseq, hlen;

	if (chunk_hdr_peek(rx.part[pi].buf, &hseq, &hlen)) {
		rx.part[pi].used = 0;
		rx.rejected++;
		ask_resend(seq, 0);
		return;
	}
	uint32_t size = (uint32_t)chunk_size(hlen);

	if (rx.part[pi].len < size) {
		return;
	}
	rx.part[pi].used = 0;
	struct chunk_hdr h;
	const uint8_t *p;

	if (chunk_decode(rx.part[pi].buf, size, &h, &p) != CHUNK_OK) {
		rx.rejected++;
		ask_resend(seq, 0);
		return;
	}
	int slot = -1;

	for (int i = 0; i < 512; i++) { /* the same record again replaces the one we hold */
		if (rx.ahead[i].used && rx.ahead[i].seq == seq) {
			slot = i;
			rx.dups++;
		}
	}
	for (int i = 0; i < 512 && slot < 0; i++) {
		if (!rx.ahead[i].used) {
			slot = i;
		}
	}
	assert(slot >= 0);
	rx.ahead[slot].used = 1;
	rx.ahead[slot].seq = seq;
	rx.ahead[slot].size = size;
	rx.ahead[slot].type = h.type;
	memcpy(rx.ahead[slot].rec, rx.part[pi].buf, size);
	rx.ahead[slot].first = seq;
	rx.ahead[slot].last = seq;
	if (h.type == CHUNK_GAP) {
		assert(is_gap(rx.ahead[slot].rec, &rx.ahead[slot].first, &rx.ahead[slot].last));
	}
	uint32_t before = rx.next;

	accept_in_order();
	if (rx.next != before) {
		rx.acks++;
		push(SYNC_OP_ACK, rx.next - 1, 0);
	}
}

/* ---------------- the link ---------------- */

struct link {
	unsigned drop, dup, corrupt, reorder; /* per mille */
	long budget;                          /* packets until the link dies; < 0 = never */
	int dead;
	uint8_t held[8 + FRAG];               /* one packet delayed behind the next */
	size_t held_n;
	long sent, stops;
	unsigned stop_pm;                     /* per mille per fragment that the phone sends STOP */
	unsigned append_pm;                   /* per mille per poll that the recorder appends */
	unsigned stale_pm;                    /* per mille per fragment that the phone repeats an old RESEND */
} lk;

static int io_tx(void *c, const uint8_t *pkt, size_t n)
{
	uint8_t b[8 + FRAG];

	(void)c;
	if (lk.dead || lk.budget == 0) {
		lk.dead = 1;
		return -ENOTCONN;
	}
	if (lk.budget > 0) {
		lk.budget--;
	}
	lk.sent++;
	if (chance(lk.drop) && pkt[0] | pkt[1] | pkt[2] | pkt[3]) { /* the end marker is never lost silently */
		return 0;
	}
	memcpy(b, pkt, n);
	if (n > 8 + 40 && chance(lk.corrupt)) { /* payload or CRC bytes only: the header is protected by the link layer */
		b[8 + 40 + rnd() % (n - 8 - 40)] ^= (uint8_t)(1u << (rnd() % 8));
	}
	if (lk.held_n) { /* deliver the delayed packet after this one */
		rx_fragment(b, n);
		rx_fragment(lk.held, lk.held_n);
		lk.held_n = 0;
	} else if (chance(lk.reorder)) {
		memcpy(lk.held, b, n);
		lk.held_n = n;
	} else {
		rx_fragment(b, n);
		if (chance(lk.dup)) {
			rx_fragment(b, n);
		}
	}
	if (chance(lk.stop_pm)) {
		push(SYNC_OP_STOP, 0, 0); /* the phone gives up mid-transfer */
	}
	if (rx.next > 1 && chance(lk.stale_pm)) {
		push(SYNC_OP_RESEND, 1 + rnd() % (rx.next - 1), 0); /* a request for something already delivered */
	}
	return 0;
}

static int io_poll(void *c, struct sync_cmd *out)
{
	(void)c;
	if (appended < limit && chance(lk.append_pm)) {
		append_one(); /* the recorder runs while we sync */
	}
	if (!cq_n) {
		return 0;
	}
	*out = cq[0];
	memmove(cq, cq + 1, (size_t)(--cq_n) * sizeof(cq[0]));
	if (out->op == SYNC_OP_ACK) {
		assert(out->a < rx.next); /* an ACK only ever follows verified, in-order records */
	}
	return 1;
}
static void io_lock(void *c) { (void)c; }
static uint32_t io_mono(void *c) { (void)c; return 0; }
static uint64_t io_rtc(void *c) { (void)c; return 0; }

static const struct sync_io io = {.tx = io_tx, .poll = io_poll, .lock = io_lock, .unlock = io_lock, .mono_ms = io_mono,
				  .rtc_ms = io_rtc, .frag = FRAG, .buf = recbuf, .buf_size = sizeof(recbuf), .st = &st};

static void run_sync(struct sync *s, uint32_t have)
{
	int r = sync_run(s, have);

	assert(r <= 1);
	if (r < 0) {
		assert(lk.dead); /* only a dead link fails a sync */
	}
	if (r == 1) {
		lk.stops++;
	}
}

/* One connection: what the firmware's sync thread does with the queue. Returns 1 when the phone is complete. */
static int connection(struct sync *s)
{
	memset(&lk.held, 0, sizeof(lk.held));
	lk.held_n = 0;
	lk.dead = 0;
	memset(rx.part, 0, sizeof(rx.part)); /* partial records do not survive a reconnect */
	memset(rx.wait, 0, sizeof(rx.wait));
	cq_n = 0;
	rx.end_seen = 0;
	push(SYNC_OP_RECLAIM, 0, 0); /* queued by the disconnect callback */
	push(SYNC_OP_SYNC, rx.next - 1, 0);
	for (int retry = 0, guard = 0; guard < 4000; guard++) {
		struct sync_cmd c;

		if (!io_poll(NULL, &c)) {
			if (lk.dead) {
				return 0;
			}
			/* the phone's timeout: sent everything but something is missing -> ask again from what it has */
			uint32_t have = 0;
			int complete = rx.end_seen && rx.next >= rx.end_next;

			if (complete) {
				return 1;
			}
			if (!rx.end_seen || retry++ >= 20) {
				return 0;
			}
			for (int i = 0; i < 8; i++) {
				if (rx.part[i].used && rx.part[i].seq == rx.next) {
					have = rx.part[i].len;
				}
			}
			ask_resend(rx.next, have);
			continue;
		}
		switch (c.op) {
		case SYNC_OP_SYNC:
			run_sync(s, c.a);
			break;
		case SYNC_OP_RESEND:
			sync_resend(s, c.a, c.b);
			break;
		default: /* ACK, reclaim, STOP between syncs: put it back and drain, like the firmware thread */
			memmove(cq + 1, cq, (size_t)cq_n * sizeof(cq[0]));
			cq[0] = c;
			cq_n++;
			sync_drain(s);
			if (s->restart) { /* a SYNC was queued behind them */
				run_sync(s, s->have);
			}
		}
	}
	assert(!"connection did not settle");
	return 0;
}

struct scn {
	const char *name;
	uint32_t nblk;            /* ring size in blocks (2 meta + data) */
	unsigned drop, dup, corrupt, reorder, stop_pm, append_pm, stale_pm;
	int disconnects;          /* random link deaths */
	int overwrite;            /* the recorder outruns the phone: gaps are expected */
	uint32_t total;           /* records the recorder writes */
	uint32_t burst;           /* records appended between connections */
};

static void run(const struct scn *sc, unsigned long long seed)
{
	struct sync s;
	long conns = 0, gaps = 0;


	ctx.seed = seed;
	ctx.scn = sc->name;
	rng_state = seed * 0x9e3779b97f4a7c15ull + 1;
	mem_size = sc->nblk * BLOCK;
	memset(mem, 0xff, mem_size);
	memset(&rx, 0, sizeof(rx));
	memset(&st, 0, sizeof(st));
	memset(&lk, 0, sizeof(lk));
	cq_n = 0;
	appended = 0;
	limit = sc->total;
	dev = (struct ring_dev){.read = f_read, .write = f_write, .erase = f_erase, .size = mem_size, .block = BLOCK,
				.page = PAGE, .wunit = 1, .pbuf = pbuf};
	assert(!ring_format(&ring, &dev, MAXREC));
	rx.next = 1;
	lk.drop = sc->drop;
	lk.dup = sc->dup;
	lk.corrupt = sc->corrupt;
	lk.reorder = sc->reorder;
	lk.stop_pm = sc->stop_pm;
	lk.append_pm = sc->append_pm;
	lk.stale_pm = sc->stale_pm;
	sync_init(&s, &ring, &io);

	int done = 0;

	while (!done && conns < 400) {
		for (uint32_t i = 0; i < sc->burst && appended < sc->total; i++) {
			append_one();
		}
		lk.budget = (sc->disconnects && conns < 60 && chance(700)) ? (long)(1 + rnd() % 300) : -1;
		lk.append_pm = conns < 60 ? sc->append_pm : 0; /* the recorder stops: let the phone catch up */
		sync_init(&s, &ring, &io);
		int complete = connection(&s);

		conns++;
		done = complete && appended >= sc->total && rx.next == ring.next_seq;
		if (conns % 7 == 0 && !done) { /* a power cycle of the pendant: the ring is recovered from flash */
			struct ring again;

			assert(!ring_open(&again, &dev, MAXREC));
			assert(again.next_seq == ring.next_seq && again.tail_seq == ring.tail_seq);
			ring = again;
		}
	}
	assert(done);

	/* every sequence number is accounted for exactly once */
	for (uint32_t q = 1; q <= appended; q++) {
		assert(rx.covered[q]);
		if (!sc->overwrite) {
			assert(rx.covered[q] == 1); /* no gaps unless the recorder lapped the phone */
		} else if (rx.covered[q] == 2) {
			gaps++;
		}
	}
	assert(rx.next == appended + 1);
	assert(sc->overwrite || rx.delivered == (long)appended);
	/* the ring survived: reopening it works and the ACK reached the end */
	struct ring again;

	assert(!ring_open(&again, &dev, MAXREC) && again.next_seq == appended + 1);
	total_stops += lk.stops;
	total_gaps += gaps;
	total_rejected += rx.rejected;
	total_resends += st.resends;
	if (seed == 1) {
		printf("  %-22s seed 1: %u records, %ld connections, %u syncs, %u resends, %ld rejected, %ld dups, %ld gaps, %ld STOPs\n",
		       sc->name, appended, conns, st.syncs, st.resends, rx.rejected, rx.dups, gaps, lk.stops);
	}
}

static void test_io_error(void)
{
	struct sync s;

	mem_size = 8 * BLOCK;
	memset(mem, 0xff, mem_size);
	dev = (struct ring_dev){.read = f_read, .write = f_write, .erase = f_erase, .size = mem_size,
				.block = BLOCK, .page = PAGE, .wunit = 1, .pbuf = pbuf};
	appended = 0;
	assert(!ring_format(&ring, &dev, MAXREC));
	append_one();
	sync_init(&s, &ring, &io);
	read_fail_after = 0;
	assert(sync_resend(&s, ring.next_seq - 1, 0) < 0);
	read_fail_after = -1;
	assert(sync_resend(&s, 0, 0) == 1); /* never emit a gap with seq=0 */
	assert(sync_resend(&s, ring.next_seq, 0) == 1);
	assert(sync_run(&s, UINT32_MAX) < 0); /* HAVE+1 would wrap to the seq=0 end marker */
	puts("  read error aborts sync; HAVE wrap is rejected; gap headers carry boot/RTC_INVALID");
}

int main(int argc, char **argv)
{
	int n = argc > 1 && !strcmp(argv[1], "quick") ? 20 : 300;
	static const struct scn scns[] = {
		{"clean", 70, 0, 0, 0, 0, 0, 0, 0, 0, 0, 300, 40},
		{"lossy", 70, 30, 20, 20, 20, 0, 300, 20, 0, 0, 300, 25},
		{"lossy+disconnects", 70, 30, 20, 20, 20, 2, 300, 20, 1, 0, 300, 25},
		{"corrupt-heavy", 70, 10, 10, 150, 10, 0, 200, 0, 1, 0, 200, 30},
		{"overwrite", 12, 20, 10, 10, 10, 2, 400, 10, 1, 1, 400, 90},
	};

	test_io_error();
	printf("sync: sender against a lossy phone model, %d seeds per scenario\n", n);
	for (size_t i = 0; i < sizeof(scns) / sizeof(scns[0]); i++) {
		total_stops = total_gaps = total_rejected = total_resends = 0;
		for (int seed = 1; seed <= n; seed++) {
			run(&scns[i], (unsigned long long)seed);
		}
		printf("    all %d seeds: %ld resends served, %ld corrupt records rejected, %ld gap records, %ld STOP aborts\n", n,
		       total_resends, total_rejected, total_gaps, total_stops);
	}
	printf("sync ok\n");
	return 0;
}
