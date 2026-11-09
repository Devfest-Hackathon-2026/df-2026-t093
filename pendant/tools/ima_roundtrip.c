/* Encode and decode s16le mono PCM (stdin) with the firmware's IMA-ADPCM, resetting the state per chunk
 * like the recorder does (8000-byte chunks = 16000 samples). Decoded PCM goes to stdout. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include "ima_adpcm.h"

#define CHUNK_SAMPLES 16000

int main(void)
{
	static int16_t in[CHUNK_SAMPLES], out[CHUNK_SAMPLES];
	static uint8_t enc[CHUNK_SAMPLES / 2];
	size_t n;

	while ((n = fread(in, sizeof(in[0]), CHUNK_SAMPLES, stdin)) > 0) {
		struct ima_adpcm e, d;

		ima_adpcm_reset(&e);
		ima_adpcm_reset(&d);
		if (n & 1) {
			in[n++] = 0; /* the packing needs an even count */
		}
		ima_adpcm_encode(&e, in, n, enc);
		ima_adpcm_decode(&d, enc, n, out);
		fwrite(out, sizeof(out[0]), n, stdout);
	}
	return 0;
}
