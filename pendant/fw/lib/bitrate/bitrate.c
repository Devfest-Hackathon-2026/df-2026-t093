#include "bitrate.h"

enum br_mode br_next(const struct br_cfg *c, enum br_mode cur, uint64_t free_bytes, uint32_t age_s)
{
	uint64_t left = age_s < c->horizon_s ? c->horizon_s - age_s : 0;
	uint64_t need = (uint64_t)c->lo_Bps * left * (100 + c->margin_pct) / 100;
	uint64_t chunk = (uint64_t)c->hi_Bps * c->chunk_s;

	if (cur == BR_LO) {
		need += c->capacity / 100 * c->resume_pct;
	}
	return free_bytes >= need + chunk ? BR_HI : BR_LO;
}
