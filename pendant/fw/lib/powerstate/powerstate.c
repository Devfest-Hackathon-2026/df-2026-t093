#include "powerstate.h"

/* ASSUME (docs/spec/power.md §4.3): CR2477 3.0 V nominal; 1.8 V rail needs the TPS62840 input above about 1.9 V
 * under the BLE TX pulse; leave 0.3 V for the pulse sag at the end of life. */
const struct ps_cfg ps_default = {
	.low_mv = 2600, .crit_mv = 2400, .term_mv = 2250, .hyst_mv = 100, .restart_mv = 2700,
	.down_samples = 3, .up_samples = 5, .term_samples = 2,
};

void ps_init(struct ps *s, enum ps_state initial)
{
	s->state = initial;
	s->down = s->up = 0;
}

/* Threshold to go below to leave `s` for the next worse state. */
static uint16_t down_mv(const struct ps_cfg *c, enum ps_state s)
{
	return s == PS_NORMAL ? c->low_mv : s == PS_LOW ? c->crit_mv : c->term_mv;
}

enum ps_state ps_update(struct ps *s, const struct ps_cfg *c, uint16_t mv)
{
	if (s->state == PS_TERMINAL) {
		return PS_TERMINAL; /* latched until reboot */
	}
	if (mv < down_mv(c, s->state)) {
		s->up = 0;
		/* the last step is quick: a flat cell gets little time to brown out the MCU */
		if (++s->down >= (s->state == PS_CRITICAL ? c->term_samples : c->down_samples)) {
			s->state++;
			s->down = 0;
		}
	} else if (s->state != PS_NORMAL && mv >= down_mv(c, s->state - 1) + c->hyst_mv) {
		s->down = 0;
		if (++s->up >= c->up_samples) {
			s->state--;
			s->up = 0;
		}
	} else {
		s->down = s->up = 0;
	}
	return s->state;
}

struct ps_policy ps_policy_for(enum ps_state s)
{
	switch (s) {
	case PS_NORMAL:
		return (struct ps_policy){.erase_program = 1, .record = 1, .low_rate = 0, .ble_sync = 1, .ota = 1};
	case PS_LOW:
		return (struct ps_policy){.erase_program = 1, .record = 1, .low_rate = 1, .ble_sync = 1, .ota = 0};
	case PS_CRITICAL:
	case PS_TERMINAL:
	default:
		return (struct ps_policy){0};
	}
}

int ps_boot_ok(const struct ps_cfg *c, uint16_t mv)
{
	return mv >= c->restart_mv;
}
