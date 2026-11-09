/* Host test: make -C fw/lib powerstate. Voltage ramps in, state and policy checked out. */
#include <assert.h>
#include <stdio.h>
#include "powerstate.h"

static uint32_t rng = 12345;
static int noise(int amp_mv)
{
	rng = rng * 1664525u + 1013904223u;
	return (int)((rng >> 16) % (2 * amp_mv + 1)) - amp_mv;
}

int main(void)
{
	const struct ps_cfg *c = &ps_default;
	struct ps s;
	int changes = 0, seen[4] = {0};
	enum ps_state prev;

	/* 1. Slow discharge 3.0 V -> 2.0 V with +-30 mV noise: each stage is entered once, in order, no chatter. */
	ps_init(&s, PS_NORMAL);
	prev = s.state;
	for (int mv = 3000; mv >= 2000; mv--) {
		ps_update(&s, c, (uint16_t)(mv + noise(30)));
		if (s.state != prev) {
			assert(s.state == prev + 1); /* one step at a time */
			changes++;
			seen[s.state]++;
			prev = s.state;
		}
	}
	assert(changes == 3 && seen[PS_LOW] == 1 && seen[PS_CRITICAL] == 1 && seen[PS_TERMINAL] == 1);

	/* 2. TERMINAL is latched: a fresh cell reading does not bring it back without a reboot. */
	for (int i = 0; i < 20; i++) {
		assert(ps_update(&s, c, 3000) == PS_TERMINAL);
	}
	assert(!ps_policy_for(PS_TERMINAL).erase_program && !ps_policy_for(PS_TERMINAL).record);

	/* 3. A short sag (one or two bad samples) does not demote. */
	ps_init(&s, PS_NORMAL);
	for (int i = 0; i < 50; i++) {
		ps_update(&s, c, i % 10 == 0 ? 2300 : 2900); /* single dip every tenth sample */
		assert(s.state == PS_NORMAL);
	}
	ps_update(&s, c, 2500);
	ps_update(&s, c, 2500);
	assert(s.state == PS_NORMAL); /* two samples are not enough (down_samples = 3) */
	assert(ps_update(&s, c, 2500) == PS_LOW);

	/* 4. Hysteresis: back to NORMAL only above low_mv + hyst for up_samples in a row. */
	for (int i = 0; i < 30; i++) {
		assert(ps_update(&s, c, c->low_mv + c->hyst_mv - 1) == PS_LOW); /* just under: stays LOW */
	}
	for (int i = 0; i < c->up_samples - 1; i++) {
		assert(ps_update(&s, c, c->low_mv + c->hyst_mv) == PS_LOW);
	}
	assert(ps_update(&s, c, c->low_mv + c->hyst_mv) == PS_NORMAL);

	/* 5. Voltage that hovers on the threshold does not toggle: noise of +-60 mV around low_mv. */
	ps_init(&s, PS_NORMAL);
	changes = 0;
	prev = s.state;
	for (int i = 0; i < 5000; i++) {
		ps_update(&s, c, (uint16_t)(c->low_mv + noise(60)));
		changes += s.state != prev;
		prev = s.state;
	}
	assert(changes <= 1);

	/* 6. CRITICAL -> TERMINAL is fast (term_samples), and CRITICAL recovers to LOW with hysteresis. */
	ps_init(&s, PS_CRITICAL);
	assert(ps_update(&s, c, c->term_mv - 1) == PS_CRITICAL && ps_update(&s, c, c->term_mv - 1) == PS_TERMINAL);
	ps_init(&s, PS_CRITICAL);
	for (int i = 0; i < c->up_samples; i++) {
		ps_update(&s, c, c->crit_mv + c->hyst_mv);
	}
	assert(s.state == PS_LOW);

	/* 7. Policy table: what each stage allows. */
	struct ps_policy n = ps_policy_for(PS_NORMAL), l = ps_policy_for(PS_LOW), k = ps_policy_for(PS_CRITICAL);
	assert(n.erase_program && n.record && !n.low_rate && n.ble_sync && n.ota);
	assert(l.erase_program && l.record && l.low_rate && l.ble_sync && !l.ota);
	assert(!k.erase_program && !k.record && !k.ble_sync && !k.ota);

	/* 8. Boot gate: a battery that only just crossed the terminal threshold must not restart. */
	assert(!ps_boot_ok(c, c->term_mv + 100) && !ps_boot_ok(c, c->restart_mv - 1) && ps_boot_ok(c, c->restart_mv));
	assert(c->restart_mv > c->low_mv); /* restart above where the machine would immediately degrade */

	/* 9. Thresholds are ordered with room for the hysteresis. */
	assert(c->low_mv > c->crit_mv + c->hyst_mv && c->crit_mv > c->term_mv + c->hyst_mv);

	puts("powerstate: all tests passed");
	return 0;
}
