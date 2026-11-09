#include "audio_dsp.h"

#include <string.h>

/* Arithmetic right shift without relying on implementation-defined signed shifts. */
static int64_t asr(int64_t v, unsigned s)
{
	return (v - (v < 0 ? ((1LL << s) - 1) : 0)) / (1LL << s);
}

/* 63-tap Blackman-windowed sinc, fc = 3.6 kHz at 16 kHz, Q15. */
static const int16_t coeff[AUDIO_DSP_TAPS] = {
	0, 0, 0, 3, 3, -8, -11, 13, 29, -14, -59, 0, 101, 40, -146, -121,
	180, 253, -173, -441, 88, 677, 128, -943, -549, 1207, 1327, -1432,
	-2982, 1585, 10259, 14740, 10259, 1585, -2982, -1432, 1327, 1207,
	-549, -943, 128, 677, 88, -441, -173, 253, 180, -121, -146, 40, 101,
	0, -59, -14, 29, 13, -11, -8, 3, 3, 0, 0, 0,
};

void audio_dsp_decim_init(struct audio_dsp_decim *d)
{
	*d = (struct audio_dsp_decim){0};
}

int32_t audio_dsp_sign_extend24(uint32_t v)
{
	v &= 0xffffffu;
	return (v & 0x800000u) ? (int32_t)v - 0x1000000 : (int32_t)v;
}

size_t audio_dsp_decim_i2s(struct audio_dsp_decim *d, const uint32_t *frames, size_t nframes,
			   int16_t *out)
{
	size_t n = 0;

	for (size_t i = 0; i < nframes; i++) {
		memmove(&d->hist[1], &d->hist[0], sizeof(d->hist[0]) * (AUDIO_DSP_TAPS - 1));
		d->hist[0] = audio_dsp_sign_extend24(frames[2 * i]);
		if (++d->phase < 2) {
			continue;
		}
		d->phase = 0;

		int64_t acc = 0;

		for (int j = 0; j < AUDIO_DSP_TAPS; j++) {
			acc += (int64_t)d->hist[j] * coeff[j];
		}
		int32_t y = (int32_t)asr(acc, 23);

		if (y > 32767) {
			y = 32767;
		} else if (y < -32768) {
			y = -32768;
		}
		out[n++] = (int16_t)y;
	}
	return n;
}

void audio_dsp_dc_init(struct audio_dsp_dc *d)
{
	*d = (struct audio_dsp_dc){0};
}

void audio_dsp_dc_block(struct audio_dsp_dc *d, int16_t *s, size_t n)
{
	for (size_t i = 0; i < n; i++) {
		int32_t x = (int32_t)s[i] * 256;
		int32_t y = x - d->x + d->y - (int32_t)asr(d->y, 8);
		int32_t out = (int32_t)asr(y, 8);

		d->x = x;
		d->y = y;
		if (out > 32767) {
			out = 32767;
		} else if (out < -32768) {
			out = -32768;
		}
		s[i] = (int16_t)out;
	}
}
