/* Wake-on-sound control for the T5848 AAD Analog mode (issue #36, docs/spec/audio.md §4.5).
 *
 * The microphone needs a 50-200 kHz clock while THSEL is written, 768 kHz while recording,
 * and a 192 kHz settling window (>= 2 ms) before the clock can stop for AAD. Time and the
 * microphone are injected: the host test drives a scripted scene.
 *
 *   BOOT --192 kHz/THSEL--> SLEEP --WAKE--> STARTING --6 ms--> ACTIVE
 *     ^                       ^                                  |
 *     +---- 60 s reconfig ----+                                  |
 *                         STOPPING <-- quiet for hang ------------+
 *                             |
 *                             +--192 kHz 2 ms--> SLEEP
 */
#ifndef AAD_H
#define AAD_H

#include <stdint.h>

enum aad_clk {
	AAD_CLK_OFF = 0,
	AAD_CLK_MID = 192000, /* THSEL writes and the sleep transition */
	AAD_CLK_REC = 768000, /* low-power I2S */
};

enum aad_state { AAD_BOOT, AAD_SLEEP, AAD_STARTING, AAD_ACTIVE, AAD_STOPPING };

struct aad_ops {
	void *ctx;
	void (*clk)(void *ctx, uint32_t hz);                 /* I2S SCK/WS, or off */
	int (*ths)(void *ctx, uint8_t reg, uint8_t data);    /* THSEL one-wire write; 0 = ok */
	void (*record)(void *ctx, int on, uint32_t t_ms);    /* start / stop storing PCM */
	void (*event)(void *ctx, uint8_t kind, uint32_t t_ms); /* CHUNK_EV_* record */
	/* Optional current monotonic time. Used after a blocking callback so the
	 * 2 ms/6 ms timers measure wall time, not the tick's entry time. */
	uint32_t (*now)(void *ctx);
};

struct aad_cfg {
	uint16_t wakeup_ms;     /* 6: clock on to valid output (datasheet 1.7) */
	uint16_t hang_ms;       /* keep recording this long after the last loud block */
	uint16_t first_hang_ms; /* ... or this long if nothing loud was seen (false wake) */
	uint32_t loud_ms2;      /* a 100 ms block whose mean square exceeds this is "sound" */
	uint16_t boot_ms;       /* 2: 192 kHz settle before and after the AAD writes */
	uint16_t stopping_ms;   /* 2: 192 kHz settle before SCK/WS go LOW */
	uint32_t reconfig_ms;   /* 60000: recover the privacy latch, whose state FW cannot read */
	uint8_t privacy_zero_blocks;  /* consecutive all-zero SD blocks that mean "privacy ON" */
	uint8_t privacy_clear_blocks; /* consecutive non-zero blocks that clear the latch state */
	uint8_t lpf;            /* AAD A_LPF, 1..7 (1 = 4.4 kHz) */
	uint8_t th_code;        /* AAD A_TH, 0..15 (0 = 60 dB SPL) */
};

struct aad_stats {
	uint32_t wakes, false_wakes, reconfigs, privacy_events, cfg_errors, on_ms, head_gap_ms_max;
};

struct aad {
	struct aad_ops ops;
	struct aad_cfg cfg;
	enum aad_state state;
	uint32_t t_wake, t_active, t_last_loud, t_boot, t_stop, t_last_cfg;
	int saw_loud, wake_level;
	uint8_t boot_stage, boot_reconfig, configured, zero_blocks, nonzero_blocks, privacy;
	struct aad_stats st;
};

int aad_init(struct aad *a, const struct aad_ops *ops, const struct aad_cfg *cfg, uint32_t t_ms);
/* Call from a work item/thread after the GPIO interrupt, not from the ISR:
 * start() calls blocking clock/event callbacks. Only a rising edge in SLEEP acts. */
void aad_wake(struct aad *a, int level, uint32_t t_ms); /* rising edge only */
/* Call from a dedicated thread at <= 1 ms, not from an ISR: advances BOOT/STARTING/STOPPING
 * and reconfigures AAD every reconfig_ms while asleep. Do not call concurrently with
 * aad_block()/aad_sd_block() without external locking. */
void aad_tick(struct aad *a, uint32_t t_ms);
/* One 100 ms PCM block while recording, with its mean square. */
void aad_block(struct aad *a, uint32_t ms2, uint32_t t_ms);
/* One 100 ms block for the privacy-latch detector (all_zero = every SD sample is 0). */
void aad_sd_block(struct aad *a, int all_zero, uint32_t t_ms);
/* Call from a work item/thread, not an ISR. Always logged; a press wakes the recorder. */
void aad_button(struct aad *a, uint32_t t_ms);

#endif
