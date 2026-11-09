/* Host simulation of the rate controller on the real ring over a RAM flash.
 * cc -std=c99 -O2 -Wall -Wextra -Werror -I../chunk -I../ring -o /tmp/t test_bitrate.c bitrate.c ../ring/ring.c ../chunk/chunk.c && /tmp/t */
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "bitrate.h"
#include "ring.h"

#define BLOCK 262144u
#define PAGE 4096u
#define NBLK 1968u /* 515.9 MB of data blocks: the 4 Gbit usable capacity in docs/spec/storage.md §7.1 */
#define CHUNK_S 8u
#define HI_PAY 32728u /* page-aligned: 32768 B on flash */

static uint8_t *mem;
static int s_read(void *c, uint32_t a, void *b, size_t n) { (void)c; memcpy(b, mem + a, n); return 0; }
static int s_write(void *c, uint32_t a, const void *b, size_t n)
{
	(void)c;
	for (size_t i = 0; i < n; i++) {
		mem[a + i] &= ((const uint8_t *)b)[i];
	}
	return 0;
}
static int s_erase(void *c, uint32_t a) { (void)c; memset(mem + a, 0xff, BLOCK); return 0; }

struct result {
	double first_lo_h, hi_h, retained_h, occ48;
	unsigned switches;
	int dropped_by_48h, overwrote;
	uint32_t gap_first, gap_last;
	double lo_after_sync_h;
};

static uint64_t rng = 88172645463325252ULL;
static double rnd01(void)
{
	rng ^= rng << 13; rng ^= rng >> 7; rng ^= rng << 17;
	return (rng >> 11) / 9007199254740992.0;
}

/* p = share of 8 s slots with sound (stored); sync_h > 0: the phone appears at that hour and ACKs all */
static struct result run(double p, double hours, double sync_h, uint32_t hi_pay, uint32_t lo_pay, double slot_s)
{
	static uint8_t pbuf[PAGE], pay[HI_PAY];
	struct ring_dev d = {.read = s_read, .write = s_write, .erase = s_erase, .size = (NBLK + 2) * BLOCK,
			     .block = BLOCK, .page = PAGE, .wunit = PAGE, .single_program = 1, .pbuf = pbuf};
	struct br_cfg cfg = {.capacity = (uint64_t)NBLK * BLOCK, .hi_Bps = 4000, .lo_Bps = 2000, .chunk_s = CHUNK_S,
			     .horizon_s = 48 * 3600, .margin_pct = 10, .resume_pct = 5};
	struct ring r;
	struct result res = {.first_lo_h = -1};
	uint32_t *tstart = calloc(200000, sizeof(uint32_t));
	enum br_mode mode = BR_HI;
	uint32_t slots = hours * 3600 / slot_s, hi_slots = 0, lo_slots = 0;
	int synced = 0;

	mem = malloc((size_t)d.size);
	memset(mem, 0xff, d.size);
	assert(!ring_format(&r, &d, chunk_size(HI_PAY)) && !ring_open(&r, &d, chunk_size(HI_PAY)));
	for (uint32_t i = 0; i < slots; i++) {
		uint32_t t = i * slot_s;

		if (sync_h > 0 && !synced && t >= sync_h * 3600) {
			synced = 1;
			ring_ack(&r, r.next_seq - 1);
			assert(!ring_reclaim(&r));
		}
		if (i == (uint32_t)(48 * 3600 / slot_s)) {
			res.occ48 = 1.0 - (double)ring_free(&r) / cfg.capacity;
			res.dropped_by_48h = r.tail_seq - 1 > 0 && r.tail_seq > r.acked + 1;
		}
		if (rnd01() >= p) {
			continue;
		}
		uint32_t age = r.tail_seq == r.next_seq ? 0 : t - tstart[r.tail_seq];
		enum br_mode m = br_next(&cfg, mode, ring_free(&r), age);
		struct chunk_hdr h = {.type = CHUNK_AUDIO, .codec = m == BR_HI ? 1 : 2, .mono_ms = t * 1000,
				      .len = m == BR_HI ? hi_pay : lo_pay};

		if (m != mode) {
			res.switches++;
			if (m == BR_LO && res.first_lo_h < 0) {
				res.first_lo_h = t / 3600.0;
			}
		}
		mode = m;
		mode == BR_HI ? hi_slots++ : lo_slots++;
		tstart[r.next_seq] = t;
		assert(!ring_append(&r, &h, pay));
		if (synced && mode == BR_LO) {
			res.lo_after_sync_h += slot_s / 3600.0;
		}
	}
	res.hi_h = hi_slots * slot_s / 3600.0;
	res.retained_h = r.tail_seq == r.next_seq ? 0 : (hours * 3600 - tstart[r.tail_seq]) / 3600.0;
	res.overwrote = ring_gap(&r, &res.gap_first, &res.gap_last);
	free(tstart);
	free(mem);
	(void)lo_slots;
	return res;
}

static void table(const char *title, uint32_t hi, uint32_t lo, double slot, double lo_min, double lo_max)
{
	static const double ps[] = {1.0, 0.75, 0.5};
	struct result r;

	printf("%s\n   p   first LO   hi hours   occupancy at 48 h   overwrite by 48 h   switches   retained at 72 h\n", title);
	for (int k = 0; k < 3; k++) {
		r = run(ps[k], 72, 0, hi, lo, slot);
		printf("%4.2f   %6.1f h   %7.1f    %5.1f %%             %s               %u         %.1f h%s\n", ps[k],
		       r.first_lo_h, r.hi_h, r.occ48 * 100, r.dropped_by_48h ? "YES" : "no ", r.switches, r.retained_h,
		       r.overwrote ? " (oldest overwritten)" : "");
		if (ps[k] == 1.0) {
			assert(r.first_lo_h > lo_min && r.first_lo_h < lo_max);
		}
		assert(!r.dropped_by_48h);        /* acceptance: no overwrite in 48 h, even at p = 1 */
		assert(r.switches <= 3);          /* no chatter */
		if (ps[k] <= 0.5) {
			assert(r.first_lo_h < 0 || r.first_lo_h > 48); /* p <= 0.55: no degrading inside the 48 h */
		}
	}
}

/* Space freed by an ACK is free (erase-on-enter) but not erased until ring_pre_erase(). */
static void test_erased_ahead(void)
{
	static uint8_t pb[PAGE], pay[HI_PAY];
	struct ring_dev d = {.read = s_read, .write = s_write, .erase = s_erase, .size = (40 + 2) * BLOCK,
			     .block = BLOCK, .page = PAGE, .wunit = PAGE, .single_program = 1, .pbuf = pb};
	struct ring r;
	struct chunk_hdr h = {.type = CHUNK_AUDIO, .codec = 1, .len = HI_PAY};

	mem = malloc(d.size);
	memset(mem, 0xff, d.size);
	assert(!ring_format(&r, &d, chunk_size(HI_PAY)) && !ring_open(&r, &d, chunk_size(HI_PAY)));
	assert(ring_free(&r) == 40ull * BLOCK && ring_erased_ahead(&r) == 0); /* open forgets what was erased */
	assert(!ring_pre_erase(&r, 40) && ring_erased_ahead(&r) == 40ull * BLOCK);
	for (int i = 0; i < 700; i++) { /* two laps: every block has been used and is dirty */
		assert(!ring_append(&r, &h, pay));
	}
	ring_ack(&r, r.next_seq - 1);
	assert(!ring_reclaim(&r));
	assert(ring_free(&r) >= 38ull * BLOCK);          /* everything is free ... */
	assert(ring_erased_ahead(&r) < 2ull * BLOCK);    /* ... but almost none of it is erased yet */
	assert(!ring_pre_erase(&r, 10));
	assert(ring_erased_ahead(&r) >= 9ull * BLOCK && ring_erased_ahead(&r) <= ring_free(&r));
	free(mem);
}

int main(void)
{
	struct result r;

	test_erased_ahead();

	puts("rate controller on the real ring (515.9 MB, 32 -> 16 kbps), phone absent");
	table("8 s of audio per record (32000 B payload, padded to 32 KiB on flash: +2.4 %):", 32000, 16000, 8.0, 19.5, 20.5);
	table("page-aligned records (32728 B payload = 8.18 s, no padding; docs/spec/storage.md §7.3 assumes this):", HI_PAY, 16344, 8.182, 20.5,
	      21.3);
	r = run(1.0, 72, 30, HI_PAY, 16344, 8.182);
	printf("p=1.00, phone ACKs everything at 30 h: switches %u, hi hours %.1f, LO after the sync %.1f h\n",
	       r.switches, r.hi_h, r.lo_after_sync_h);
	assert(r.switches >= 2 && r.switches <= 4); /* LO, back to HI after the sync, LO again */

	struct br_cfg c = {.capacity = 1000000000ULL, .hi_Bps = 4000, .lo_Bps = 2000, .chunk_s = 8,
			   .horizon_s = 172800, .margin_pct = 10, .resume_pct = 5};
	uint32_t age = 40 * 3600;
	uint64_t need = 2000ULL * (172800 - age) * 110 / 100 + 32000;

	assert(br_next(&c, BR_HI, need, age) == BR_HI && br_next(&c, BR_HI, need - 1, age) == BR_LO);
	/* hysteresis: back to HI only with 5 % of capacity to spare, not at the same threshold */
	assert(br_next(&c, BR_LO, need, age) == BR_LO && br_next(&c, BR_LO, need + 50000000, age) == BR_HI);
	assert(br_next(&c, BR_HI, 0, 200000) == BR_LO && br_next(&c, BR_HI, 32000, 200000) == BR_HI); /* past the horizon */
	puts("bitrate: all tests passed");
	return 0;
}
