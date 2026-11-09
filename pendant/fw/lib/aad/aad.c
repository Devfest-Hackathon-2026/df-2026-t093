#include "aad.h"
#include "chunk.h"
#include "t5838.h"

static uint32_t now(struct aad *a, uint32_t fallback)
{
	return a->ops.now ? a->ops.now(a->ops.ctx) : fallback;
}

static void boot_start(struct aad *a, uint32_t t, int reconfig)
{
	a->state = AAD_BOOT;
	a->boot_stage = 0;
	a->boot_reconfig = reconfig;
	a->configured = 0; /* no audible path until the THSEL sequence succeeds */
	a->ops.clk(a->ops.ctx, AAD_CLK_MID);
	a->t_boot = now(a, t); /* clk may block; measure the settle after it returns */
}

static int write_cfg(struct aad *a)
{
	struct t5838_reg c[3];
	size_t n;

	if (!a->ops.ths) {
		return -1;
	}
	for (size_t i = 0; i < sizeof(t5838_unlock) / sizeof(t5838_unlock[0]); i++) {
		if (a->ops.ths(a->ops.ctx, t5838_unlock[i].reg, t5838_unlock[i].data)) {
			return -1;
		}
	}
	n = t5838_aad_a_cfg(c, a->cfg.lpf, a->cfg.th_code);
	for (size_t i = 0; i < n; i++) {
		if (a->ops.ths(a->ops.ctx, c[i].reg, c[i].data)) {
			return -1;
		}
	}
	return 0;
}

int aad_init(struct aad *a, const struct aad_ops *ops, const struct aad_cfg *cfg, uint32_t t_ms)
{
	if (!a || !ops || !cfg || !ops->clk || !ops->ths || !ops->record || !ops->event) {
		return -1;
	}
	*a = (struct aad){.ops = *ops, .cfg = *cfg, .state = AAD_BOOT};
	if (a->cfg.wakeup_ms < 6) {
		a->cfg.wakeup_ms = 6;
	}
	if (a->cfg.hang_ms == 0) {
		a->cfg.hang_ms = 2000;
	}
	if (a->cfg.first_hang_ms == 0) {
		a->cfg.first_hang_ms = 300;
	}
	if (a->cfg.loud_ms2 == 0) {
		a->cfg.loud_ms2 = 1000;
	}
	if (a->cfg.boot_ms < 2) {
		a->cfg.boot_ms = 2;
	}
	if (a->cfg.stopping_ms < 2) {
		a->cfg.stopping_ms = 2;
	}
	if (a->cfg.reconfig_ms < 1000) {
		a->cfg.reconfig_ms = 60000;
	}
	if (a->cfg.privacy_zero_blocks == 0) {
		a->cfg.privacy_zero_blocks = 3;
	}
	if (a->cfg.privacy_clear_blocks == 0) {
		a->cfg.privacy_clear_blocks = 2;
	}
	if (a->cfg.lpf < 1 || a->cfg.lpf > 7) {
		a->cfg.lpf = 1;
	}
	if (a->cfg.th_code > 0xf) {
		a->cfg.th_code = 0;
	}
	boot_start(a, t_ms, 0);
	return 0;
}

static void start(struct aad *a, uint32_t t)
{
	a->state = AAD_STARTING;
	a->st.wakes++;
	a->ops.clk(a->ops.ctx, AAD_CLK_REC);
	a->t_wake = now(a, t); /* the 6 ms starts when the clock is actually on */
	a->ops.event(a->ops.ctx, CHUNK_EV_AAD_WAKE, t);
}

void aad_wake(struct aad *a, int level, uint32_t t)
{
	int rising = level && !a->wake_level;

	a->wake_level = level;
	if (rising && a->state == AAD_SLEEP && a->configured) {
		start(a, t);
	}
}

void aad_tick(struct aad *a, uint32_t t)
{
	uint32_t tn = now(a, t);

	switch (a->state) {
	case AAD_BOOT:
		if (a->boot_stage == 0 && tn - a->t_boot >= a->cfg.boot_ms) {
			if (write_cfg(a) == 0) {
				a->boot_stage = 1;
				a->configured = 1;
				a->t_boot = now(a, t); /* writes can take several ms; settle after them */
			} else {
				/* Fail closed: no AAD means no recording. SCK must not stay on. */
				a->st.cfg_errors++;
				a->configured = 0;
				a->ops.clk(a->ops.ctx, AAD_CLK_OFF);
				a->state = AAD_SLEEP;
				a->boot_reconfig = 0;
				a->t_last_cfg = now(a, t);
			}
		} else if (a->boot_stage == 1 && tn - a->t_boot >= a->cfg.boot_ms) {
			a->ops.clk(a->ops.ctx, AAD_CLK_OFF);
			a->state = AAD_SLEEP;
			a->t_last_cfg = now(a, t);
			if (a->boot_reconfig) {
				a->st.reconfigs++;
				a->boot_reconfig = 0;
			}
		}
		break;
	case AAD_STARTING:
		if (tn - a->t_wake >= a->cfg.wakeup_ms) {
			a->state = AAD_ACTIVE;
			a->ops.record(a->ops.ctx, 1, t);
			a->t_active = now(a, t);
			a->t_last_loud = a->t_active;
			a->saw_loud = 0;
			a->zero_blocks = 0;
			a->nonzero_blocks = 0;
			a->privacy = 0;
			if (a->t_active - a->t_wake > a->st.head_gap_ms_max) {
				a->st.head_gap_ms_max = a->t_active - a->t_wake;
			}
		}
		break;
	case AAD_ACTIVE:
		break;
	case AAD_STOPPING:
		if (tn - a->t_stop >= a->cfg.stopping_ms) {
			a->ops.clk(a->ops.ctx, AAD_CLK_OFF);
			a->ops.event(a->ops.ctx, CHUNK_EV_AAD_SLEEP, tn);
			a->st.on_ms += tn - a->t_wake;
			a->st.false_wakes += !a->saw_loud;
			a->state = AAD_SLEEP;
			a->t_last_cfg = now(a, t);
		}
		break;
	case AAD_SLEEP:
		/* 60 s is far below 2^31, so unsigned subtraction survives the 32-bit wrap. */
		if (a->cfg.reconfig_ms && tn - a->t_last_cfg >= a->cfg.reconfig_ms) {
			boot_start(a, tn, 1);
		}
		break;
	}
}

void aad_block(struct aad *a, uint32_t ms2, uint32_t t)
{
	uint32_t hang;
	uint32_t tn = now(a, t);

	if (a->state != AAD_ACTIVE) {
		return;
	}
	if (ms2 > a->cfg.loud_ms2) {
		a->t_last_loud = tn;
		a->saw_loud = 1;
	}
	hang = a->saw_loud ? a->cfg.hang_ms : a->cfg.first_hang_ms;
	if (tn - a->t_last_loud >= hang) {
		a->ops.record(a->ops.ctx, 0, tn);
		a->ops.clk(a->ops.ctx, AAD_CLK_MID); /* settle before SCK/WS go LOW */
		a->state = AAD_STOPPING;
		a->t_stop = now(a, tn);
	}
}

void aad_sd_block(struct aad *a, int all_zero, uint32_t t)
{
	if (a->state != AAD_ACTIVE && a->state != AAD_STOPPING) {
		a->zero_blocks = 0;
		a->nonzero_blocks = 0;
		return;
	}
	if (!all_zero) {
		a->zero_blocks = 0;
		if (!a->privacy) {
			a->nonzero_blocks = 0;
			return;
		}
		if (++a->nonzero_blocks >= a->cfg.privacy_clear_blocks) {
			a->privacy = 0;
			a->nonzero_blocks = 0;
		}
		return;
	}
	a->nonzero_blocks = 0;
	if (a->privacy) {
		return;
	}
	if (++a->zero_blocks >= a->cfg.privacy_zero_blocks) {
		a->zero_blocks = 0;
		a->privacy = 1;
		a->st.privacy_events++;
		a->ops.event(a->ops.ctx, CHUNK_EV_PRIVACY_ON, t);
	}
}

void aad_button(struct aad *a, uint32_t t)
{
	a->ops.event(a->ops.ctx, CHUNK_EV_BUTTON, t);
	if (a->state == AAD_SLEEP && a->configured) {
		start(a, t);
	}
}
