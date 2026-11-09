/* Backlog-linked recording rate (docs/spec/storage.md §7.3, issue #5): record at the high rate while there is
 * room to still survive `horizon` at the floor rate, otherwise drop to the floor rate. Pure C. */
#ifndef BITRATE_H
#define BITRATE_H

#include <stdint.h>

enum br_mode { BR_HI, BR_LO };

struct br_cfg {
	uint64_t capacity;   /* usable bytes of the ring */
	uint32_t hi_Bps;     /* 4000 = 32 kbps */
	uint32_t lo_Bps;     /* 2000 = 16 kbps (placeholder: the degraded codec is not decided, docs/spec/audio.md §6.2) */
	uint32_t chunk_s;    /* record length in seconds; modes switch only between records */
	uint32_t horizon_s;  /* 48 h */
	uint32_t margin_pct; /* 10: keep 1.1x the floor-rate need */
	uint32_t resume_pct; /* 5: return to hi only with this much capacity to spare (hysteresis) */
};

/* Rate for the next record.
 *   free_bytes: ring_free()  age_s: seconds since the oldest unacked record started (0 if none) */
enum br_mode br_next(const struct br_cfg *c, enum br_mode cur, uint64_t free_bytes, uint32_t age_s);

#endif
