#include "ima_adpcm.h"

static const int8_t index_table[16] = {
	-1, -1, -1, -1, 2, 4, 6, 8, -1, -1, -1, -1, 2, 4, 6, 8,
};

static const int16_t step_table[89] = {
	7, 8, 9, 10, 11, 12, 13, 14, 16, 17, 19, 21, 23, 25, 28, 31,
	34, 37, 41, 45, 50, 55, 60, 66, 73, 80, 88, 97, 107, 118, 130, 143,
	157, 173, 190, 209, 230, 253, 279, 307, 337, 371, 408, 449, 494, 544, 598, 658,
	724, 796, 876, 963, 1060, 1166, 1282, 1411, 1552, 1707, 1878, 2066, 2272, 2499, 2749, 3024,
	3327, 3660, 4026, 4428, 4871, 5358, 5894, 6484, 7132, 7845, 8630, 9493, 10442, 11487, 12635, 13899,
	15289, 16818, 18500, 20350, 22385, 24623, 27086, 29794, 32767,
};

/* Shared by both sides so encoder and decoder predictors never diverge. */
static void update(struct ima_adpcm *s, uint8_t code)
{
	int step = step_table[s->index];
	int diff = step >> 3;
	int pred = s->pred;
	int index = s->index + index_table[code];

	if (code & 4) {
		diff += step;
	}
	if (code & 2) {
		diff += step >> 1;
	}
	if (code & 1) {
		diff += step >> 2;
	}
	pred += (code & 8) ? -diff : diff;
	s->pred = pred > 32767 ? 32767 : pred < -32768 ? -32768 : pred;
	s->index = index < 0 ? 0 : index > 88 ? 88 : index;
}

static uint8_t encode_one(struct ima_adpcm *s, int16_t sample)
{
	int step = step_table[s->index];
	int diff = sample - s->pred;
	uint8_t code = 0;

	if (diff < 0) {
		code = 8;
		diff = -diff;
	}
	if (diff >= step) {
		code |= 4;
		diff -= step;
	}
	step >>= 1;
	if (diff >= step) {
		code |= 2;
		diff -= step;
	}
	if (diff >= step >> 1) {
		code |= 1;
	}
	update(s, code);
	return code;
}

void ima_adpcm_encode(struct ima_adpcm *s, const int16_t *pcm, size_t n, uint8_t *out)
{
	for (size_t i = 0; i < n; i += 2) {
		uint8_t b = encode_one(s, pcm[i]);

		if (i + 1 < n) {
			b |= encode_one(s, pcm[i + 1]) << 4;
		}
		*out++ = b;
	}
}

void ima_adpcm_decode(struct ima_adpcm *s, const uint8_t *in, size_t n, int16_t *pcm)
{
	for (size_t i = 0; i < n; i++) {
		update(s, (in[i / 2] >> ((i & 1) * 4)) & 0x0f);
		pcm[i] = s->pred;
	}
}
