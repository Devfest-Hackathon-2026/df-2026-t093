/* Interop harness (mobile/ tests): the real pendant-side sender, fw/lib/{sync,ring,chunk} linked read-only, on a RAM
 * ring. The phone side (Kotlin) drives it over stdin/stdout, in lock step, one text line per message:
 *
 *   in : INIT <data_blocks> <seed> | REC <n> | INFO | STATE | SYNC <have> | RESEND <seq> <off> | CMD <hex> | DISC | QUIT
 *   out: OK | I <hex32> | S <tail> <next> <acked> | D <hex fragment> | DONE <rc> | POLL
 *   after POLL the phone answers with any number of  C <hex ctrl> | R <n>  lines, then  END
 *
 * Build: cc -std=c99 -O2 -Wall -Wextra -Werror -I../../fw/lib/chunk -I../../fw/lib/ring -I../../fw/lib/sync harness.c \
 *        ../../fw/lib/sync/sync.c ../../fw/lib/ring/ring.c ../../fw/lib/chunk/chunk.c */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "sync.h"

#define BLOCK 4096u
#define PAGE 256u
#define MAXPAY 1000u
#define MAXREC ((uint32_t)(CHUNK_HDR_SIZE + MAXPAY + CHUNK_TRAILER_SIZE))
#define FRAG 244u
#define MAXBLK 80

static uint8_t mem[MAXBLK * BLOCK];
static uint32_t mem_size;
static struct ring ring;
static struct ring_dev dev;
static uint8_t pbuf[PAGE], recbuf[MAXREC];
static struct sync_stats st;
static uint64_t rng_state = 88172645463325252ull;
static int rtc_set;
static uint64_t rtc_base;
static uint32_t mono;

static uint32_t rnd(void)
{
	rng_state ^= rng_state << 13;
	rng_state ^= rng_state >> 7;
	rng_state ^= rng_state << 17;
	return (uint32_t)(rng_state >> 11);
}

static int f_read(void *c, uint32_t a, void *b, size_t n)
{
	(void)c;
	if (a + n > mem_size) {
		return -1;
	}
	memcpy(b, mem + a, n);
	return 0;
}

static int f_write(void *c, uint32_t a, const void *b, size_t n)
{
	(void)c;
	if (a + n > mem_size) {
		return -1;
	}
	for (size_t i = 0; i < n; i++) {
		if (((const uint8_t *)b)[i] & ~mem[a + i]) {
			return -1; /* program only clears bits */
		}
		mem[a + i] &= ((const uint8_t *)b)[i];
	}
	return 0;
}

static int f_erase(void *c, uint32_t a)
{
	(void)c;
	if (a % BLOCK || a + BLOCK > mem_size) {
		return -1;
	}
	memset(mem + a, 0xff, BLOCK);
	return 0;
}

static void die(const char *m)
{
	fprintf(stderr, "harness: %s\n", m);
	exit(2);
}

static void hexout(const uint8_t *p, size_t n)
{
	for (size_t i = 0; i < n; i++) {
		printf("%02x", p[i]);
	}
}

static size_t hexin(const char *s, uint8_t *out, size_t max)
{
	size_t n = 0;
	unsigned v;

	while (n < max && sscanf(s + 2 * n, "%2x", &v) == 1) {
		out[n++] = (uint8_t)v;
	}
	return n;
}

static void append_one(void)
{
	static uint8_t pay[MAXPAY];
	uint32_t len = 100 + (rnd() % 225) * 4, seq = ring.next_seq;
	struct chunk_hdr h = {.type = CHUNK_AUDIO, .codec = CHUNK_CODEC_IMA4_8K, .mono_ms = mono, .len = len,
			      .flags = rtc_set ? 0 : CHUNK_F_RTC_INVALID, .rtc_ms = rtc_set ? rtc_base + mono : 0};

	mono += 500;
	for (uint32_t i = 0; i < len; i++) {
		pay[i] = (uint8_t)(seq * 31u + i * 7u + 5u);
	}
	if (ring_append(&ring, &h, pay) || h.seq != seq) {
		die("ring_append failed");
	}
}

/* ---------------- sync_io ---------------- */

static int io_tx(void *c, const uint8_t *pkt, size_t n)
{
	(void)c;
	printf("D ");
	hexout(pkt, n);
	printf("\n");
	fflush(stdout);
	return 0;
}

static struct sync_cmd cq[256];
static int cq_n, cq_head, asked;

static void push(const uint8_t *b, size_t n)
{
	struct sync_cmd c = {.op = b[0]};

	if (cq_n >= 256 || n < 1) {
		die("command queue");
	}
	if ((c.op == SYNC_OP_ACK || c.op == SYNC_OP_SYNC || c.op == SYNC_OP_RESEND) && n >= 5) {
		c.a = b[1] | b[2] << 8 | b[3] << 16 | (uint32_t)b[4] << 24;
	}
	if (c.op == SYNC_OP_RESEND && n >= 9) {
		c.b = b[5] | b[6] << 8 | b[7] << 16 | (uint32_t)b[8] << 24;
	}
	if (c.op == SYNC_OP_TIME && n >= 9) {
		for (int i = 7; i >= 0; i--) {
			c.t = c.t << 8 | b[1 + i];
		}
	}
	cq[cq_n++] = c;
}

static char line[8192];

static void ask_phone(void)
{
	printf("POLL\n");
	fflush(stdout);
	while (fgets(line, sizeof(line), stdin)) {
		uint8_t b[64];

		if (!strncmp(line, "END", 3)) {
			return;
		}
		if (!strncmp(line, "C ", 2)) {
			push(b, hexin(line + 2, b, sizeof(b)));
		} else if (!strncmp(line, "R ", 2)) {
			for (int n = atoi(line + 2); n > 0; n--) {
				append_one();
			}
		}
	}
	die("stdin closed in poll");
}

static int io_poll(void *c, struct sync_cmd *out)
{
	(void)c;
	if (cq_head == cq_n) {
		cq_head = cq_n = 0;
		if (asked) {
			asked = 0;
			return 0;
		}
		asked = 1;
		ask_phone();
	}
	if (cq_head == cq_n) {
		asked = 0;
		return 0;
	}
	*out = cq[cq_head++];
	return 1;
}

static void io_other(void *c, const struct sync_cmd *cm) /* TIME, as ble.c does it */
{
	uint8_t p[CHUNK_TIMEFIX_SIZE];
	struct chunk_hdr h = {.type = CHUNK_TIMEFIX, .mono_ms = mono, .len = sizeof(p), .rtc_ms = cm->t};

	(void)c;
	chunk_timefix_encode(p, rtc_set ? rtc_base + mono : 0, cm->t);
	rtc_set = 1;
	rtc_base = cm->t - mono;
	if (ring_append(&ring, &h, p)) {
		die("timefix append");
	}
}

static void nolock(void *c) { (void)c; }
static uint32_t io_mono(void *c) { (void)c; return mono; }
static uint64_t io_rtc(void *c) { (void)c; return rtc_set ? rtc_base + mono : 0; }

static struct sync_io io = {.tx = io_tx, .poll = io_poll, .other = io_other, .lock = nolock, .unlock = nolock,
			    .mono_ms = io_mono, .rtc_ms = io_rtc, .frag = FRAG, .buf = recbuf, .buf_size = sizeof(recbuf), .st = &st};

static void drain_idle(void)
{
	struct sync s;

	sync_init(&s, &ring, &io);
	asked = 1; /* between commands there is nobody to ask: the queue is all there is */
	sync_drain(&s);
	asked = 0;
}

int main(void)
{
	while (fgets(line, sizeof(line), stdin)) {
		unsigned a, b;
		uint8_t buf[64];
		struct sync s;

		if (sscanf(line, "INIT %u %llu", &a, (unsigned long long *)&rng_state) == 2) {
			if (a + 2 > MAXBLK) {
				die("too many blocks");
			}
			mem_size = (a + 2) * BLOCK;
			memset(mem, 0xff, sizeof(mem));
			dev = (struct ring_dev){.read = f_read, .write = f_write, .erase = f_erase, .size = mem_size, .block = BLOCK,
						.page = PAGE, .wunit = 1, .pbuf = pbuf};
			if (ring_format(&ring, &dev, MAXREC) || ring_open(&ring, &dev, MAXREC)) {
				die("ring init");
			}
			printf("OK\n");
		} else if (sscanf(line, "REC %u", &a) == 1) {
			while (a--) {
				append_one();
			}
			printf("OK\n");
		} else if (!strncmp(line, "INFO", 4)) {
			uint8_t d[32] = {1, 1};
			uint64_t t = rtc_set ? rtc_base + mono : 0;
			uint32_t v[5] = {ring.boot, ring.tail_seq, ring.next_seq, ring.acked, mono / 1000};

			for (int i = 0; i < 5; i++) {
				for (int k = 0; k < 4; k++) {
					d[4 + 4 * i + k] = (uint8_t)(v[i] >> (8 * k));
				}
			}
			for (int k = 0; k < 8; k++) {
				d[24 + k] = (uint8_t)(t >> (8 * k));
			}
			printf("I ");
			hexout(d, 32);
			printf("\n");
		} else if (!strncmp(line, "STATE", 5)) {
			printf("S %u %u %u\n", ring.tail_seq, ring.next_seq, ring.acked);
		} else if (sscanf(line, "SYNC %u", &a) == 1) {
			int rc;

			sync_init(&s, &ring, &io);
			asked = 0;
			rc = sync_run(&s, a);
			printf("DONE %d\n", rc);
		} else if (sscanf(line, "RESEND %u %u", &a, &b) == 2) {
			int rc;

			sync_init(&s, &ring, &io);
			rc = sync_resend(&s, a, b);
			printf("DONE %d\n", rc);
		} else if (!strncmp(line, "CMD ", 4)) {
			push(buf, hexin(line + 4, buf, sizeof(buf)));
			drain_idle();
			printf("OK\n");
		} else if (!strncmp(line, "DISC", 4)) {
			buf[0] = SYNC_OP_STOP;
			push(buf, 1);
			buf[0] = SYNC_OP_RECLAIM;
			push(buf, 1);
			drain_idle();
			printf("OK\n");
		} else if (!strncmp(line, "QUIT", 4)) {
			return 0;
		}
		fflush(stdout);
	}
	return 0;
}
