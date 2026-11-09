/* IMA (DVI) ADPCM, 4 bit/sample. Pure C, no Zephyr dependency.
 * Packing: two samples per byte, first sample in the low nibble (WAV IMA order). */
#ifndef IMA_ADPCM_H
#define IMA_ADPCM_H

#include <stddef.h>
#include <stdint.h>

struct ima_adpcm {
	int16_t pred;
	uint8_t index;
};

/* Call at every chunk start on both sides so a chunk decodes on its own (docs/spec/formats.md §2). */
static inline void ima_adpcm_reset(struct ima_adpcm *s)
{
	s->pred = 0;
	s->index = 0;
}

static inline size_t ima_adpcm_bytes(size_t samples)
{
	return (samples + 1) / 2;
}

void ima_adpcm_encode(struct ima_adpcm *s, const int16_t *pcm, size_t n, uint8_t *out);
void ima_adpcm_decode(struct ima_adpcm *s, const uint8_t *in, size_t n, int16_t *pcm);

#endif
