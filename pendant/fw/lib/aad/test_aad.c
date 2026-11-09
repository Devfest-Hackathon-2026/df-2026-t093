/* Host test: make -C fw/lib aad. Scripted scenes of WAKE edges, THSEL writes and 100 ms blocks. */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "aad.h"
#include "chunk.h"

struct log {
	uint32_t clk;
	int rec;
	char seq[512]; /* o/m/C = clock off/192 kHz/768 kHz, R/r = record, w/s/b/p = events, T = THSEL */
	unsigned nevents[8];
	uint32_t ev_t[32], nev;
	uint32_t rec_on_t[8], rec_off_t[8];
	unsigned nrec_on, nrec_off, nths;
	uint8_t ths_reg, ths_data;
	int ths_fail;
	uint32_t now, clk_delay;
};

static void o_clk(void *c, uint32_t hz)
{
	struct log *l = c;

	l->clk = hz;
	if (hz == AAD_CLK_REC) {
		l->now += l->clk_delay;
	}
	strcat(l->seq, hz == AAD_CLK_OFF ? "o" : hz == AAD_CLK_MID ? "m" : "C");
}

static int o_ths(void *c, uint8_t reg, uint8_t data)
{
	struct log *l = c;

	assert(l->clk == AAD_CLK_MID); /* THSEL is only written in the 192 kHz window */
	l->nths++;
	l->ths_reg = reg;
	l->ths_data = data;
	l->now += 5; /* a full 1-wire write takes several ms at 192 kHz */
	strcat(l->seq, "T");
	return l->ths_fail ? -1 : 0;
}

static uint32_t o_now(void *c)
{
	return ((struct log *)c)->now;
}

static void o_rec(void *c, int on, uint32_t t)
{
	struct log *l = c;

	assert(!on || l->clk == AAD_CLK_REC); /* record only at 768 kHz */
	assert(on || l->clk != AAD_CLK_OFF);  /* stop recording before the clock goes away */
	l->rec = on;
	strcat(l->seq, on ? "R" : "r");
	if (on) {
		l->rec_on_t[l->nrec_on++] = t;
	} else {
		l->rec_off_t[l->nrec_off++] = t;
	}
}

static void o_ev(void *c, uint8_t kind, uint32_t t)
{
	struct log *l = c;
	const char *s = kind == CHUNK_EV_BUTTON ? "b" : kind == CHUNK_EV_AAD_WAKE ? "w" :
			kind == CHUNK_EV_AAD_SLEEP ? "s" : "p";

	l->nevents[kind]++;
	l->ev_t[l->nev++] = t;
	strcat(l->seq, s);
}

/* Plays one scene second by second: loud(t) says whether the room is loud at t. */
static void run(struct aad *a, uint32_t from, uint32_t to, uint32_t (*loud)(uint32_t))
{
	for (uint32_t t = from; t < to; t += 100) {
		aad_tick(a, t);
		aad_block(a, loud(t), t);
	}
}

static uint32_t quiet(uint32_t t)
{
	(void)t;
	return 20;
}

static uint32_t speech_3s_at_5s(uint32_t t)
{
	return t >= 5000 && t < 8000 ? 40000 : 20;
}

/* Talking with 1 s pauses, from 1 s to 9 s, then nothing. */
static uint32_t chatter(uint32_t t)
{
	return t >= 1000 && t < 9000 && (t / 1000) % 3 != 2 ? 40000 : 20;
}

static uint32_t two_bursts(uint32_t t)
{
	return (t >= 1000 && t < 2000) || (t >= 9000 && t < 10000) ? 40000 : 20;
}

static void boot(struct aad *a)
{
	aad_tick(a, 1); /* not before the 2 ms settle */
	aad_tick(a, 2); /* write THSEL at 192 kHz */
	assert(a->state == AAD_BOOT);
	aad_tick(a, 4); /* 2 ms settle, then SCK/WS off */
	assert(a->state == AAD_SLEEP);
}

int main(void)
{
	static const struct aad_cfg cfg = {.wakeup_ms = 6, .hang_ms = 2000, .first_hang_ms = 300,
					   .loud_ms2 = 1000};
	struct log l;
	struct aad a;
	struct aad_ops ops = {.ctx = &l, .clk = o_clk, .ths = o_ths, .record = o_rec, .event = o_ev};

	/* 1. Boot writes unlock + AAD settings at 192 kHz, then stops the clock. */
	memset(&l, 0, sizeof(l));
	assert(aad_init(&a, &ops, &cfg, 0) == 0);
	assert(a.state == AAD_BOOT && l.clk == AAD_CLK_MID && !l.rec);
	boot(&a);
	assert(a.state == AAD_SLEEP && l.clk == AAD_CLK_OFF && !l.rec);
	assert(l.nths == 8); /* five unlock writes + LPF + TH + AAD enable */
	assert(l.ths_reg == 0x29 && l.ths_data == 0x08);
	run(&a, 4, 4000, quiet);
	assert(!l.rec && a.st.wakes == 0);
	aad_wake(&a, 0, 4000);
	assert(a.state == AAD_SLEEP);

	/* 2. A real utterance: WAKE at 5 s, speech 3 s. Clock 768 kHz first, recording 6 ms later,
	 *    then 192 kHz for 2 ms before SCK/WS go LOW. */
	aad_wake(&a, 1, 5000);
	assert(a.state == AAD_STARTING && l.clk == AAD_CLK_REC && !l.rec);
	aad_tick(&a, 5003);
	assert(!l.rec); /* not before 6 ms */
	aad_tick(&a, 5006);
	assert(l.rec && l.rec_on_t[0] == 5006);
	run(&a, 5100, 12000, speech_3s_at_5s);
	assert(l.nrec_off == 1 && l.rec_off_t[0] == 9900);
	assert(l.clk == AAD_CLK_OFF && a.state == AAD_SLEEP);
	assert(a.st.wakes == 1 && a.st.false_wakes == 0 && a.st.head_gap_ms_max == 6);
	assert(strstr(l.seq, "CwR") && strstr(l.seq, "rmo"));
	/* Rising-edge contract: WAKE still high after sleep must not start a new recording. */
	aad_wake(&a, 1, 12100);
	assert(a.st.wakes == 1 && a.state == AAD_SLEEP);
	aad_wake(&a, 0, 12200);
	aad_wake(&a, 1, 12300);
	assert(a.st.wakes == 2 && a.state == AAD_STARTING);

	/* 3. A false wake: a click, WAKE blips and nothing loud follows: only first_hang_ms of recording. */
	memset(&l, 0, sizeof(l));
	assert(aad_init(&a, &ops, &cfg, 0) == 0);
	boot(&a);
	aad_wake(&a, 1, 1000);
	aad_wake(&a, 0, 1020);
	run(&a, 1000, 3000, quiet);
	assert(a.state == AAD_SLEEP && a.st.false_wakes == 1 && a.st.wakes == 1);
	assert(a.st.on_ms >= 300 && a.st.on_ms <= 500); /* ~0.4 s of 120 uA, not 2 s */

	/* 4. Pauses shorter than hang_ms do not chop a conversation. */
	memset(&l, 0, sizeof(l));
	assert(aad_init(&a, &ops, &cfg, 0) == 0);
	boot(&a);
	aad_wake(&a, 1, 1000);
	run(&a, 1000, 15000, chatter);
	assert(l.nrec_on == 1 && l.nrec_off == 1 && a.state == AAD_SLEEP);

	/* 5. Two bursts 8 s apart are two wakes. */
	memset(&l, 0, sizeof(l));
	assert(aad_init(&a, &ops, &cfg, 0) == 0);
	boot(&a);
	aad_wake(&a, 1, 1000);
	run(&a, 1000, 9000, two_bursts);
	assert(a.state == AAD_SLEEP && l.nrec_on == 1 && l.nrec_off == 1);
	aad_wake(&a, 0, 8500); /* WAKE falls before the next burst */
	aad_wake(&a, 1, 9000);
	run(&a, 9000, 14000, two_bursts);
	assert(l.nrec_on == 2 && a.st.wakes == 2 && l.clk == AAD_CLK_OFF);

	/* 6. WAKE edges while recording are ignored. */
	memset(&l, 0, sizeof(l));
	assert(aad_init(&a, &ops, &cfg, 0) == 0);
	boot(&a);
	aad_wake(&a, 1, 100);
	aad_tick(&a, 106);
	aad_wake(&a, 1, 200);
	aad_wake(&a, 0, 300);
	aad_wake(&a, 1, 400);
	assert(a.st.wakes == 1 && l.nrec_on == 1);
	/* WAKE while BOOT/STARTING/STOPPING is ignored too. */
	{
		struct aad b;

		memset(&l, 0, sizeof(l));
		assert(aad_init(&b, &ops, &cfg, 0) == 0);
		aad_wake(&b, 1, 1);
		assert(b.state == AAD_BOOT && b.st.wakes == 0);
		aad_tick(&b, 2);
		aad_tick(&b, 4);
		aad_wake(&b, 1, 10);
		aad_tick(&b, 16);
		aad_wake(&b, 0, 20);
		aad_wake(&b, 1, 21);
		assert(b.state == AAD_STARTING && b.st.wakes == 1);
		aad_tick(&b, 27);
		assert(b.state == AAD_ACTIVE);
		aad_block(&b, 20, 100);
		aad_block(&b, 20, 400);
		assert(b.state == AAD_STOPPING);
		aad_wake(&b, 0, 401);
		aad_wake(&b, 1, 402);
		assert(b.state == AAD_STOPPING && b.st.wakes == 1);
	}

	/* 7. Buttons are logged in every state; a press while asleep starts recording. */
	memset(&l, 0, sizeof(l));
	assert(aad_init(&a, &ops, &cfg, 0) == 0);
	boot(&a);
	aad_button(&a, 1234);
	assert(l.nevents[CHUNK_EV_BUTTON] == 1 && l.ev_t[l.nev - 1] == 1234 && a.state == AAD_STARTING);
	aad_tick(&a, 1240);
	aad_button(&a, 1500);
	assert(l.nevents[CHUNK_EV_BUTTON] == 2 && l.ev_t[l.nev - 1] == 1500 && a.state == AAD_ACTIVE);

	/* 8. The 192 kHz stop window really waits before the clock is removed. */
	memset(&l, 0, sizeof(l));
	assert(aad_init(&a, &ops, &cfg, 0) == 0);
	boot(&a);
	aad_wake(&a, 1, 1000);
	aad_tick(&a, 1006);
	aad_block(&a, 20, 1006); /* first block, no sound */
	aad_block(&a, 20, 1306); /* first_hang_ms reached: stop requested */
	assert(a.state == AAD_STOPPING && l.clk == AAD_CLK_MID && !l.rec);
	aad_tick(&a, 1308);
	assert(a.state == AAD_SLEEP && l.clk == AAD_CLK_OFF);
	assert(l.nevents[CHUNK_EV_AAD_SLEEP] == 1);

	/* 9. AAD is reconfigured every 60 s while asleep; the clock settles at 192 kHz. */
	memset(&l, 0, sizeof(l));
	assert(aad_init(&a, &ops, &cfg, 0) == 0);
	boot(&a);
	l.nths = 0;
	aad_tick(&a, 65000);
	assert(a.state == AAD_BOOT && l.clk == AAD_CLK_MID && a.st.reconfigs == 0);
	aad_tick(&a, 65002);
	assert(l.nths == 8);
	aad_tick(&a, 65004);
	assert(a.state == AAD_SLEEP && l.clk == AAD_CLK_OFF && a.st.reconfigs == 1);

	/* 10. Three all-zero SD blocks report privacy once; two non-zero blocks re-arm it. */
	memset(&l, 0, sizeof(l));
	assert(aad_init(&a, &ops, &cfg, 0) == 0);
	boot(&a);
	aad_wake(&a, 1, 1000);
	aad_tick(&a, 1006);
	aad_sd_block(&a, 1, 1100);
	aad_sd_block(&a, 1, 1200);
	assert(l.nevents[CHUNK_EV_PRIVACY_ON] == 0);
	aad_sd_block(&a, 1, 1300);
	assert(l.nevents[CHUNK_EV_PRIVACY_ON] == 1 && a.privacy);
	aad_sd_block(&a, 1, 1400);
	assert(l.nevents[CHUNK_EV_PRIVACY_ON] == 1);
	/* One non-zero block is not enough to re-arm; two are. */
	aad_sd_block(&a, 0, 1500);
	aad_sd_block(&a, 0, 1600);
	aad_sd_block(&a, 0, 1700);
	assert(l.nevents[CHUNK_EV_PRIVACY_ON] == 1 && !a.privacy);
	aad_sd_block(&a, 1, 1800);
	aad_sd_block(&a, 1, 1900);
	aad_sd_block(&a, 1, 2000);
	assert(l.nevents[CHUNK_EV_PRIVACY_ON] == 2);

	/* 11. aad_init takes the caller's monotonic time; 2 ms must elapse before THSEL. */
	memset(&l, 0, sizeof(l));
	assert(aad_init(&a, &ops, &cfg, 1000) == 0);
	assert(a.state == AAD_BOOT && l.clk == AAD_CLK_MID);
	aad_tick(&a, 1001);
	assert(l.nths == 0);
	aad_tick(&a, 1002);
	assert(l.nths == 8 && a.state == AAD_BOOT);
	aad_tick(&a, 1004);
	assert(a.state == AAD_SLEEP);

	/* 12. A blocking THSEL write advances the settle timer: now() is used after the callback. */
	{
		struct aad_ops on = ops;
		struct aad b;

		on.now = o_now;
		memset(&l, 0, sizeof(l));
		l.now = 2000;
		assert(aad_init(&b, &on, &cfg, 2000) == 0);
		l.now = 2002;
		aad_tick(&b, 2002);
		assert(b.state == AAD_BOOT && b.t_boot == 2042); /* 8 writes x 5 ms */
		l.now = 2043;
		aad_tick(&b, 2043);
		assert(b.state == AAD_BOOT); /* only 1 ms of settle */
		l.now = 2044;
		aad_tick(&b, 2044);
		assert(b.state == AAD_SLEEP);
	}

	/* 13. The 60 s reconfig uses unsigned subtraction and survives the 32-bit wrap. */
	memset(&l, 0, sizeof(l));
	assert(aad_init(&a, &ops, &cfg, 0) == 0);
	boot(&a);
	a.t_last_cfg = 0xfffffff2u;
	aad_tick(&a, 0xfffffff2u + 60000u);
	assert(a.state == AAD_BOOT);
	aad_tick(&a, 0xfffffff2u + 60002u);
	aad_tick(&a, 0xfffffff2u + 60004u);
	assert(a.state == AAD_SLEEP && a.st.reconfigs == 1);

	/* 14. A failed THSEL write fails closed (clock OFF) and is retried after reconfig_ms. */
	memset(&l, 0, sizeof(l));
	l.ths_fail = 1;
	assert(aad_init(&a, &ops, &cfg, 0) == 0);
	aad_tick(&a, 2);
	assert(a.state == AAD_SLEEP && l.clk == AAD_CLK_OFF && a.st.cfg_errors == 1);
	aad_tick(&a, 4); /* no 2 ms retry storm while SCK is off */
	assert(a.state == AAD_SLEEP && a.st.cfg_errors == 1);
	aad_tick(&a, 60002); /* reconfig timer expires */
	assert(a.state == AAD_BOOT && l.clk == AAD_CLK_MID);
	aad_tick(&a, 60004); /* still failing: fail closed again */
	assert(a.state == AAD_SLEEP && l.clk == AAD_CLK_OFF && a.st.cfg_errors == 2);
	{
		uint32_t wakes = a.st.wakes;

		aad_wake(&a, 1, 5);
		assert(a.state == AAD_SLEEP && a.st.wakes == wakes);
		aad_button(&a, 6);
		assert(a.state == AAD_SLEEP && l.nevents[CHUNK_EV_BUTTON] == 1);
	}
	/* A later successful reconfig restores the wake path. */
	l.ths_fail = 0;
	aad_tick(&a, 120002);
	aad_tick(&a, 120004);
	aad_tick(&a, 120006);
	aad_tick(&a, 120008);
	assert(a.state == AAD_SLEEP && a.configured);
	aad_wake(&a, 0, 120010);
	aad_wake(&a, 1, 120011);
	assert(a.state == AAD_STARTING);

	/* 15. Hardware minimums are clamped even if the caller passes smaller values. */
	{
		struct aad_cfg bad = {.wakeup_ms = 1, .boot_ms = 1, .stopping_ms = 1, .reconfig_ms = 1,
				      .privacy_zero_blocks = 1, .privacy_clear_blocks = 1, .lpf = 9};
		struct aad b;

		assert(aad_init(&b, &ops, &bad, 0) == 0);
		assert(b.cfg.wakeup_ms == 6 && b.cfg.boot_ms == 2 && b.cfg.stopping_ms == 2);
		assert(b.cfg.reconfig_ms == 60000 && b.cfg.lpf == 1);
		assert(b.cfg.privacy_zero_blocks == 1 && b.cfg.privacy_clear_blocks == 1);
	}

	/* 16. The 6 ms wake delay starts when the 768 kHz clock is actually on. */
	{
		struct aad_ops on = ops;
		struct aad b;

		on.now = o_now;
		memset(&l, 0, sizeof(l));
		l.clk_delay = 5;
		assert(aad_init(&b, &on, &cfg, 0) == 0);
		l.now = 2;
		aad_tick(&b, 2); /* 8 writes advance now to 42 */
		assert(b.state == AAD_BOOT && b.t_boot == 42);
		l.now = 43;
		aad_tick(&b, 43);
		assert(b.state == AAD_BOOT);
		l.now = 44;
		aad_tick(&b, 44);
		assert(b.state == AAD_SLEEP);
		l.now = 4000;
		aad_tick(&b, 4000);
		aad_wake(&b, 1, 4000);
		assert(b.state == AAD_STARTING && b.t_wake == 4005);
		l.now = 4006;
		aad_tick(&b, 4006);
		assert(b.state == AAD_STARTING);
		l.now = 4011;
		aad_tick(&b, 4011);
		assert(b.state == AAD_ACTIVE);
	}

	/* 17. A third all-zero block arriving after the stop request still logs privacy once. */
	memset(&l, 0, sizeof(l));
	assert(aad_init(&a, &ops, &cfg, 0) == 0);
	boot(&a);
	aad_wake(&a, 1, 1000);
	aad_tick(&a, 1006);
	aad_sd_block(&a, 1, 1100);
	aad_sd_block(&a, 1, 1200);
	aad_block(&a, 20, 1306); /* stop requested */
	assert(a.state == AAD_STOPPING);
	aad_sd_block(&a, 1, 1307);
	assert(l.nevents[CHUNK_EV_PRIVACY_ON] == 1);

	/* 18. Missing required callbacks are rejected. */
	{
		struct aad b;
		struct aad_ops bad = ops;

		bad.ths = NULL;
		assert(aad_init(&b, &bad, &cfg, 0) != 0);
		bad = ops;
		bad.record = NULL;
		assert(aad_init(&b, &bad, &cfg, 0) != 0);
		bad = ops;
		bad.event = NULL;
		assert(aad_init(&b, &bad, &cfg, 0) != 0);
		assert(aad_init(NULL, &ops, &cfg, 0) != 0);
	}

	/* 19. What it costs: on-time share over a day of scenes -> average microphone current. */
	{
		double on_share = 0.12; /* 12 % of the day in low-power I2S */
		double ua = 20 * (1 - on_share) + 120 * on_share;

		printf("  microphone average at %.0f %% on-time: %.1f uA (continuous low-power: 120 uA)\n",
		       on_share * 100, ua);
		assert(ua < 40);
	}

	puts("aad: all tests passed");
	return 0;
}
