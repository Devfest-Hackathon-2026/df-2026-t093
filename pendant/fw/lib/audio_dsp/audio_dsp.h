#ifndef AUDIO_DSP_H
#define AUDIO_DSP_H

#include <stddef.h>
#include <stdint.h>

#define AUDIO_DSP_IN_RATE  16000
#define AUDIO_DSP_OUT_RATE 8000
#define AUDIO_DSP_TAPS     63

struct audio_dsp_decim {
	int32_t hist[AUDIO_DSP_TAPS];
	uint8_t phase;
};

struct audio_dsp_dc {
	int32_t x, y;
};

void audio_dsp_decim_init(struct audio_dsp_decim *d);
int32_t audio_dsp_sign_extend24(uint32_t v);
size_t audio_dsp_decim_i2s(struct audio_dsp_decim *d, const uint32_t *frames, size_t nframes,
			   int16_t *out);

void audio_dsp_dc_init(struct audio_dsp_dc *d);
void audio_dsp_dc_block(struct audio_dsp_dc *d, int16_t *s, size_t n);

#endif
