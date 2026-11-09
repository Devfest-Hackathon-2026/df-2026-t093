/* Heartbeat supervisor (docs/spec/firmware.md §4.1, issue #11): the watchdog is fed only while every task that is
 * supposed to be alive has reported recently. Pure C, time is an argument. */
#ifndef HEALTH_H
#define HEALTH_H

#include <stdint.h>

#define HEALTH_MAX 8

struct health {
	uint32_t timeout_ms[HEALTH_MAX]; /* 0 = not supervised right now (idle, or not started) */
	uint32_t last_ms[HEALTH_MAX];
	uint32_t stuck;                  /* bit i set once task i was found late (for the reset log) */
};

void health_init(struct health *h);
/* Start supervising task id: it must beat within timeout_ms from now on. */
void health_arm(struct health *h, unsigned id, uint32_t timeout_ms, uint32_t now_ms);
/* The task is idle by design (e.g. recording stopped): do not wait for it. */
void health_disarm(struct health *h, unsigned id);
void health_beat(struct health *h, unsigned id, uint32_t now_ms);
/* 1 if every armed task is within its timeout (feed the watchdog), else 0 and `stuck` says who. */
int health_ok(struct health *h, uint32_t now_ms);

#endif
