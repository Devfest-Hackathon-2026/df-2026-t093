#include <string.h>
#include "health.h"

void health_init(struct health *h)
{
	memset(h, 0, sizeof(*h));
}

void health_arm(struct health *h, unsigned id, uint32_t timeout_ms, uint32_t now_ms)
{
	if (id < HEALTH_MAX) {
		h->timeout_ms[id] = timeout_ms;
		h->last_ms[id] = now_ms;
	}
}

void health_disarm(struct health *h, unsigned id)
{
	if (id < HEALTH_MAX) {
		h->timeout_ms[id] = 0;
	}
}

void health_beat(struct health *h, unsigned id, uint32_t now_ms)
{
	if (id < HEALTH_MAX) {
		h->last_ms[id] = now_ms;
	}
}

int health_ok(struct health *h, uint32_t now_ms)
{
	int ok = 1;

	for (unsigned i = 0; i < HEALTH_MAX; i++) {
		if (h->timeout_ms[i] && now_ms - h->last_ms[i] > h->timeout_ms[i]) { /* wraps correctly */
			h->stuck |= 1u << i;
			ok = 0;
		}
	}
	return ok;
}
