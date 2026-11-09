#include <string.h>
#include "t5838.h"

#define STOP_CLK 130

const struct t5838_reg t5838_unlock[5] = {{0x5c, 0}, {0x3e, 0}, {0x6f, 0}, {0x3b, 0}, {0x4c, 0}};

size_t t5838_write_pulses(struct t5838_pulse *out, size_t max, uint8_t reg, uint8_t data, unsigned tp)
{
	const uint8_t bytes[3] = {T5838_DEV_ADDR_W, reg, data};
	size_t n = 0;

	if (tp < 8 || tp > 20 || max < T5838_MAX_PULSES) {
		return 0;
	}
	out[n++] = (struct t5838_pulse){1, tp}; /* start / pilot */
	for (int b = 0; b < 24; b++) {
		int bit = bytes[b / 8] >> (7 - b % 8) & 1;

		out[n++] = (struct t5838_pulse){0, tp};              /* space */
		out[n++] = (struct t5838_pulse){1, bit ? 3 * tp : tp}; /* zero = 1 tp, one = 3 tp */
	}
	out[n++] = (struct t5838_pulse){0, tp};
	out[n++] = (struct t5838_pulse){1, STOP_CLK};
	return n;
}

int t5838_decode_pulses(const struct t5838_pulse *p, size_t n, uint8_t *reg, uint8_t *data, unsigned *tpo)
{
	uint32_t v = 0;
	unsigned tp;
	size_t i = 0;

	if (n != T5838_MAX_PULSES || !p[0].high || p[0].clk < 8 || p[0].clk > 20) {
		return -1;
	}
	tp = p[0].clk;
	for (int b = 0; b < 24; b++) {
		const struct t5838_pulse *sp = &p[1 + 2 * b], *bit = &p[2 + 2 * b];

		/* space: low, 1 tp to 2 tp. symbol: high, zero 1..1.5 tp, one 2..3.5 tp (x2 to stay in integers) */
		if (sp->high || sp->clk < tp || sp->clk > 2 * tp || !bit->high) {
			return -1;
		}
		uint64_t twice = 2ULL * bit->clk;

		if (bit->clk >= tp && twice <= 3ULL * tp) {
			v = v << 1;
		} else if (bit->clk >= 2ULL * tp && twice <= 7ULL * tp) {
			v = v << 1 | 1;
		} else {
			return -1;
		}
		i = 2 + 2 * b;
	}
	if (p[i + 1].high || p[i + 1].clk < tp || !p[i + 2].high || p[i + 2].clk <= 128) {
		return -1; /* space, then a stop longer than 128 CLK */
	}
	if ((v >> 16) != T5838_DEV_ADDR_W) {
		return -1;
	}
	*reg = v >> 8;
	*data = v;
	if (tpo) {
		*tpo = tp;
	}
	return 0;
}

size_t t5838_aad_a_cfg(struct t5838_reg *out, uint8_t lpf, uint8_t th_code)
{
	if (lpf < 1 || lpf > 7 || th_code > 0xf) {
		return 0;
	}
	out[0] = (struct t5838_reg){0x35, lpf};
	out[1] = (struct t5838_reg){0x36, th_code};
	out[2] = (struct t5838_reg){0x29, 0x08}; /* AAD A enable */
	return 3;
}

uint8_t t5838_aad_a_code(unsigned db_spl_x10)
{
	static const struct { uint16_t db10; uint8_t code; } t[] = {
		{600, 0x0}, {650, 0x2}, {700, 0x4}, {750, 0x6}, {800, 0x8}, {850, 0xa}, {900, 0xc}, {950, 0xe}, {975, 0xf},
	};

	for (unsigned i = 0; i < sizeof(t) / sizeof(t[0]); i++) {
		if (db_spl_x10 <= t[i].db10) {
			return t[i].code;
		}
	}
	return 0xff;
}

enum t5838_mode t5838_mode_for_clk(uint32_t hz)
{
	if (hz == 0) {
		return T5838_SLEEP;
	}
	if (hz >= 50000 && hz <= 200000) {
		return T5838_AAD_WRITE;
	}
	if (hz >= 400000 && hz <= 800000) {
		return T5838_LOW_POWER;
	}
	if (hz >= 2000000 && hz <= 3700000) {
		return T5838_HIGH_QUALITY;
	}
	if (hz >= 4200000 && hz <= 4800000) {
		return T5838_ULTRASONIC;
	}
	return T5838_BAD;
}
