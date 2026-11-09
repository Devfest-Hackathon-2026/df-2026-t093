/* Heartbeat supervisor + hardware watchdog (docs/spec/firmware.md §4, issue #11). */
#ifndef SUPERVISOR_H
#define SUPERVISOR_H

#include <stdint.h>

enum sup_task { SUP_AUDIO, SUP_WRITER, SUP_BLE, SUP_MAIN };

int supervisor_init(void); /* 0 = WDT ready (or disabled), <0 = WDT requested but unavailable */
void sup_arm(enum sup_task t, uint32_t timeout_ms);
void sup_disarm(enum sup_task t);
void sup_beat(enum sup_task t);

/* Reset cause of this boot as CHUNK_RESET_*, read and cleared once. */
uint8_t supervisor_reset_cause(void);

#endif
