/* Host test: make -C fw/lib t5838 */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "t5838.h"

int main(void)
{
	struct t5838_pulse p[T5838_MAX_PULSES];
	uint8_t reg, data;
	unsigned tp;

	/* 1. The datasheet's example (100 kHz CLK, 10-cycle pilot = 100 us): zero 100 us, one 300 us, stop > 1280 us. */
	assert(t5838_write_pulses(p, T5838_MAX_PULSES, 0x01, 0x02, 10) == T5838_MAX_PULSES);
	assert(p[0].high && p[0].clk == 10);                 /* pilot */
	assert(!p[1].high && p[1].clk == 10);                /* space */
	assert(p[2].high && p[2].clk == 30);                 /* first bit of 0xA6 = 1 -> one, 3 x TP */
	assert(p[4].high && p[4].clk == 10);                 /* second bit 0 -> zero, 1 x TP */
	assert(p[T5838_MAX_PULSES - 1].high && p[T5838_MAX_PULSES - 1].clk > 128);
	assert(!t5838_decode_pulses(p, T5838_MAX_PULSES, &reg, &data, &tp) && reg == 0x01 && data == 0x02 && tp == 10);

	/* 2. Every register/data pair round-trips, for the whole pilot range. */
	for (unsigned t = 8; t <= 20; t++) {
		for (unsigned v = 0; v < 65536; v += 257) {
			assert(t5838_write_pulses(p, T5838_MAX_PULSES, v >> 8, v & 255, t));
			assert(!t5838_decode_pulses(p, T5838_MAX_PULSES, &reg, &data, &tp) && reg == (v >> 8) && data == (v & 255) && tp == t);
		}
	}

	/* 3. The decoder is strict: out-of-range symbols, a short stop, a wrong device address and a bad pilot fail. */
	assert(t5838_write_pulses(p, T5838_MAX_PULSES, 0x35, 1, 10));
	{
		struct t5838_pulse q[T5838_MAX_PULSES];

		memcpy(q, p, sizeof(q));
		q[2].clk = 17; /* 1.7 tp: neither a zero (<= 1.5) nor a one (>= 2) */
		assert(t5838_decode_pulses(q, T5838_MAX_PULSES, &reg, &data, &tp));
		memcpy(q, p, sizeof(q));
		q[T5838_MAX_PULSES - 1].clk = 128; /* stop must be longer than 128 CLK */
		assert(t5838_decode_pulses(q, T5838_MAX_PULSES, &reg, &data, &tp));
		memcpy(q, p, sizeof(q));
		q[0].clk = 7;
		assert(t5838_decode_pulses(q, T5838_MAX_PULSES, &reg, &data, &tp));
		memcpy(q, p, sizeof(q));
		q[3].clk = 25; /* a space of 2.5 tp */
		assert(t5838_decode_pulses(q, T5838_MAX_PULSES, &reg, &data, &tp));
		memcpy(q, p, sizeof(q));
		q[2].high = 0;
		assert(t5838_decode_pulses(q, T5838_MAX_PULSES, &reg, &data, &tp));
		assert(!t5838_write_pulses(p, T5838_MAX_PULSES, 0, 0, 7) && !t5838_write_pulses(p, T5838_MAX_PULSES, 0, 0, 21));
		/* A 2^31-cycle high pulse must not wrap 2*clk into a valid symbol. */
		assert(t5838_write_pulses(p, T5838_MAX_PULSES, 0x35, 1, 10));
		p[2].clk = 0x80000000u;
		assert(t5838_decode_pulses(p, T5838_MAX_PULSES, &reg, &data, &tp));
	}

	/* 4. The datasheet's AAD A example: unlock x5, LPF 0x1 (4.4 kHz), TH 0x4 (70 dB SPL), enable 0x08. */
	{
		struct t5838_reg c[3];

		assert(t5838_unlock[0].reg == 0x5c && t5838_unlock[4].reg == 0x4c && t5838_unlock[2].data == 0);
		assert(t5838_aad_a_cfg(c, 1, t5838_aad_a_code(700)) == 3);
		assert(c[0].reg == 0x35 && c[0].data == 1 && c[1].reg == 0x36 && c[1].data == 0x4 && c[2].reg == 0x29 && c[2].data == 0x08);
		assert(!t5838_aad_a_cfg(c, 0, 0) && !t5838_aad_a_cfg(c, 8, 0) && !t5838_aad_a_cfg(c, 1, 16));
	}

	/* 5. Threshold lookup: never below the wanted level; 60 dB is the floor, 97.5 dB the ceiling. */
	assert(t5838_aad_a_code(500) == 0x0 && t5838_aad_a_code(600) == 0x0 && t5838_aad_a_code(601) == 0x2);
	assert(t5838_aad_a_code(650) == 0x2 && t5838_aad_a_code(951) == 0xf && t5838_aad_a_code(975) == 0xf && t5838_aad_a_code(980) == 0xff);

	/* 6. CLK -> mode. 768 kHz is low power, off is sleep with AAD A running, 100 kHz is a write window. */
	assert(t5838_mode_for_clk(0) == T5838_SLEEP && t5838_mode_for_clk(100000) == T5838_AAD_WRITE);
	assert(t5838_mode_for_clk(768000) == T5838_LOW_POWER && t5838_mode_for_clk(400000) == T5838_LOW_POWER &&
	       t5838_mode_for_clk(800000) == T5838_LOW_POWER && t5838_mode_for_clk(2400000) == T5838_HIGH_QUALITY &&
	       t5838_mode_for_clk(4800000) == T5838_ULTRASONIC);
	assert(t5838_mode_for_clk(1000000) == T5838_BAD && t5838_mode_for_clk(30000) == T5838_BAD);

	puts("t5838: all tests passed");
	return 0;
}
