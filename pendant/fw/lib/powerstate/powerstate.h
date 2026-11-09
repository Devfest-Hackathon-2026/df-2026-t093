/* Standard (CR2477) supply state machine, issue #12. Pure C: the battery voltage is an input.
 * ALL THRESHOLDS ARE PLACEHOLDERS (docs/spec/power.md §4.2). They need the CR2477 discharge curve, the pulse sag
 * and the TPS62840 headroom measured on the Phase 1 carrier before they mean anything. */
#ifndef POWERSTATE_H
#define POWERSTATE_H

#include <stdint.h>

enum ps_state {
	PS_NORMAL,   /* everything allowed */
	PS_LOW,      /* record at the low rate, no OTA */
	PS_CRITICAL, /* no new erase/program, metadata committed, recording stopped, no BLE */
	PS_TERMINAL, /* shut the rail down; leaves only by a reboot with enough voltage */
};

struct ps_cfg {
	uint16_t low_mv, crit_mv, term_mv; /* enter a worse state below these */
	uint16_t hyst_mv;                  /* come back only above threshold + hyst */
	uint16_t restart_mv;               /* boot gate: stay off below this */
	uint8_t down_samples, up_samples, term_samples;
};

struct ps {
	enum ps_state state;
	uint8_t down, up; /* consecutive samples pointing the same way */
};

struct ps_policy {
	uint8_t erase_program, record, low_rate, ble_sync, ota;
};

extern const struct ps_cfg ps_default;

void ps_init(struct ps *s, enum ps_state initial);
/* Feed one battery sample (mV), taken while the radio and flash are idle, so a pulse sag is not
 * mistaken for the cell. Returns the (possibly new) state. */
enum ps_state ps_update(struct ps *s, const struct ps_cfg *c, uint16_t mv);
struct ps_policy ps_policy_for(enum ps_state s);
/* At boot: false = stay off (battery just swapped to a flat one, or still recovering). */
int ps_boot_ok(const struct ps_cfg *c, uint16_t mv);

#endif
