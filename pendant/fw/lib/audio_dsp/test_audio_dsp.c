#include "audio_dsp.h"

#include <assert.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define FRAMES 16000
#define OUT_SAMPLES (FRAMES / 2)

static uint32_t frames[2 * FRAMES];
static int16_t out_a[OUT_SAMPLES];
static int16_t out_b[OUT_SAMPLES];

static double rms(const int16_t *s, size_t n)
{
	double sum = 0;

	for (size_t i = 0; i < n; i++) {
		sum += (double)s[i] * s[i];
	}
	return sqrt(sum / n);
}

static void fill_tone(double hz, double right_hz)
{
	for (size_t i = 0; i < FRAMES; i++) {
		double l = 0.5 * 8388607.0 * sin(2.0 * M_PI * hz * i / AUDIO_DSP_IN_RATE);
		double r = 0.5 * 8388607.0 * sin(2.0 * M_PI * right_hz * i / AUDIO_DSP_IN_RATE);

		frames[2 * i] = (uint32_t)(int32_t)l & 0xffffffu;
		frames[2 * i + 1] = (uint32_t)(int32_t)r & 0xffffffu;
	}
}

static void test_sign_extend(void)
{
	assert(audio_dsp_sign_extend24(0x800000u) == -8388608);
	assert(audio_dsp_sign_extend24(0x7fffffu) == 8388607);
	assert(audio_dsp_sign_extend24(0xffffffffu) == -1);
	assert(audio_dsp_sign_extend24(1u) == 1);
}

static void test_decimation_and_block_boundary(void)
{
	struct audio_dsp_decim d;

	fill_tone(1000.0, 5000.0);
	audio_dsp_decim_init(&d);
	assert(audio_dsp_decim_i2s(&d, frames, FRAMES, out_a) == OUT_SAMPLES);

	audio_dsp_decim_init(&d);
	size_t off = 0;

	for (size_t i = 0; i < FRAMES; i += 1600) {
		off += audio_dsp_decim_i2s(&d, &frames[2 * i], 1600, &out_b[off]);
	}
	assert(off == OUT_SAMPLES);
	assert(memcmp(out_a, out_b, sizeof(out_a)) == 0);

	double pass = rms(&out_a[1000], OUT_SAMPLES - 1000) / 32768.0;

	printf("1 kHz output RMS: %.4f FS\n", pass);
	assert(pass > 0.34 && pass < 0.37);
}

static void test_stopband(void)
{
	struct audio_dsp_decim d;

	fill_tone(5000.0, 0.0);
	audio_dsp_decim_init(&d);
	assert(audio_dsp_decim_i2s(&d, frames, FRAMES, out_a) == OUT_SAMPLES);

	double stop = rms(&out_a[1000], OUT_SAMPLES - 1000) / 32768.0;

	printf("5 kHz output RMS: %.6f FS\n", stop);
	assert(stop < 0.002);
}

static void test_dc_block(void)
{
	struct audio_dsp_dc dc;
	int16_t s[8000];

	for (size_t i = 0; i < 8000; i++) {
		s[i] = 16384;
	}
	audio_dsp_dc_init(&dc);
	audio_dsp_dc_block(&dc, s, 8000);

	for (size_t i = 7900; i < 8000; i++) {
		assert(s[i] > -10 && s[i] < 10);
	}

	for (size_t i = 0; i < 8000; i++) {
		s[i] = -16384;
	}
	audio_dsp_dc_init(&dc);
	audio_dsp_dc_block(&dc, s, 8000);

	for (size_t i = 7900; i < 8000; i++) {
		assert(s[i] > -10 && s[i] < 10);
	}
}

int main(void)
{
	test_sign_extend();
	test_decimation_and_block_boundary();
	test_stopband();
	test_dc_block();
	puts("audio_dsp: all tests passed");
	return 0;
}
