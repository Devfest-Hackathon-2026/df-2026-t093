/* Writes fixtures/sample.rec (records back to back, as the C ring would store them) and
 * fixtures/sample.pcm (what the C decoder returns for the audio records), for test_rx.py.
 * cc -std=c99 -I../fw/lib/chunk -I../fw/lib/ima_adpcm -o /tmp/gen gen_fixture.c ../fw/lib/chunk/chunk.c ../fw/lib/ima_adpcm/ima_adpcm.c */
#include <math.h>
#include <stdio.h>
#include "chunk.h"
#include "ima_adpcm.h"

#define N 800 /* 100 ms */

static void put(FILE *f, uint8_t type, uint32_t seq, uint32_t boot, uint32_t mono, uint64_t rtc, uint8_t flags,
		const uint8_t *p, uint32_t len)
{
	uint8_t buf[2048];
	struct chunk_hdr h = {.type = type, .codec = type == CHUNK_AUDIO, .flags = flags, .seq = seq, .boot = boot,
			      .mono_ms = mono, .rtc_ms = rtc, .len = len};

	fwrite(buf, chunk_encode(buf, &h, p, 1), 1, f);
}

int main(void)
{
	FILE *f = fopen("fixtures/sample.rec", "wb"), *o = fopen("fixtures/sample.pcm", "wb");
	int16_t pcm[N], dec[N];
	uint8_t enc[N / 2], tp[CHUNK_TIMEFIX_SIZE], gp[CHUNK_GAP_SIZE];
	struct ima_adpcm s;
	uint32_t seq = 1;
	/* record 1-2: boot 3, clock not set (RTC_INVALID); 3: timefix; 4: gap 5..6; 7: audio after the gap */
	for (int r = 0; r < 2; r++, seq++) {
		for (int i = 0; i < N; i++) {
			pcm[i] = (int16_t)(8000 * sin(2 * 3.14159265 * 440 * (r * N + i) / 8000.0));
		}
		ima_adpcm_reset(&s);
		ima_adpcm_encode(&s, pcm, N, enc);
		put(f, CHUNK_AUDIO, seq, 3, 1000 + r * 100, 0, CHUNK_F_RTC_INVALID, enc, sizeof(enc));
		ima_adpcm_reset(&s);
		ima_adpcm_decode(&s, enc, N, dec);
		fwrite(dec, 2, N, o);
	}
	chunk_timefix_encode(tp, 0, 1760000000000ULL);
	put(f, CHUNK_TIMEFIX, seq++, 3, 1300, 1760000000000ULL, 0, tp, sizeof(tp));
	chunk_gap_encode(gp, 4, 5, CHUNK_GAP_OVERWRITTEN);
	put(f, CHUNK_GAP, 4, 3, 1300, 1760000000000ULL, 0, gp, sizeof(gp));
	seq = 6;
	for (int i = 0; i < N; i++) {
		pcm[i] = (int16_t)(8000 * sin(2 * 3.14159265 * 880 * i / 8000.0));
	}
	ima_adpcm_reset(&s);
	ima_adpcm_encode(&s, pcm, N, enc);
	put(f, CHUNK_AUDIO, seq, 3, 1700, 1760000000400ULL, 0, enc, sizeof(enc));
	ima_adpcm_reset(&s);
	ima_adpcm_decode(&s, enc, N, dec);
	fwrite(dec, 2, N, o);
	fclose(f);
	fclose(o);
	/* backwards.rec: the RTC was set back in the middle of a boot. Order must stay by sequence. */
	f = fopen("fixtures/backwards.rec", "wb");
	for (uint32_t r = 0; r < 3; r++) {
		ima_adpcm_reset(&s);
		ima_adpcm_encode(&s, pcm, N, enc);
		/* rtc: 2026-10-14 00:00:00, then 00:00:00.1 later but wound back an hour, then forward again */
		static const uint64_t rtc[3] = {1791936000000ULL, 1791932400100ULL, 1791932400200ULL};

		put(f, CHUNK_AUDIO, r + 1, 5, 1000 + r * 100, rtc[r], 0, enc, sizeof(enc));
	}
	chunk_timefix_encode(tp, 1791932400300ULL, 1791936000300ULL); /* the phone put it right again */
	put(f, CHUNK_TIMEFIX, 4, 5, 1300, 1791936000300ULL, 0, tp, sizeof(tp));
	fclose(f);
	return 0;
}
