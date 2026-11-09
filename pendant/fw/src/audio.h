/* Audio source -> 8 kHz mono PCM -> IMA-ADPCM 4 bit -> chunk buffers (source chosen in Kconfig). */
#ifndef AUDIO_H
#define AUDIO_H

#include <stdint.h>

struct audio_stats {
	uint32_t blocks, chunks, read_errors, no_buffer;
	int32_t last_min, last_max;
	uint32_t last_rms2; /* mean square of the last block */
};

/* How much of the chain runs, so the measurement build can separate the costs (docs/plan/prototype.md §2.4). */
enum audio_mode { AUDIO_PDM_ONLY, AUDIO_ENCODE, AUDIO_FULL };
void audio_set_mode(enum audio_mode m);

int audio_start(void);
void audio_stop(void);
int audio_running(void);
int audio_faulted(void); /* the thread stopped itself; leave the supervisor armed for reset */
const struct audio_stats *audio_stats(void);

#endif
