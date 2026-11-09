/* RV-3032-C7 over an I2C register callback, and a drift/jump watcher. Issue #9, docs/spec/firmware.md §5.5.
 * Why not Zephyr's rtc_rv3032: its mfd init clears PORF / VLF, the flags that tell us the time was
 * lost. Here nothing clears them until the phone has set the time. */
#ifndef RV3032_H
#define RV3032_H

#include <stddef.h>
#include <stdint.h>

#define RV_I2C_ADDR 0x51 /* 7 bit; the datasheet's A2h / A3h */

struct rv_bus {
	void *ctx;
	/* Burst access: the register address auto-increments. Return 0 or a negative errno. */
	int (*read)(void *ctx, uint8_t reg, uint8_t *buf, size_t n);
	int (*write)(void *ctx, uint8_t reg, const uint8_t *buf, size_t n);
};

enum { RV_STATUS_VLF = 0x01, RV_STATUS_PORF = 0x02 };

/* Time lost or never set: PORF or VLF is set (the flags stay until the time is set again). */
int rv_status(const struct rv_bus *b, uint8_t *status);

/* docs/spec/firmware.md §5.3 requires backup switchover and the trickle charger off (BSM = 00, TCM = 00; delivery
 * default). -EPERM if the EEPROM says otherwise: fixing it needs the EEPROM procedure, which this
 * driver does not do. Call once at boot, before trusting the chip. */
int rv_config_ok(const struct rv_bus *b);

/* UTC unix ms from 00h..07h read in one burst. If the 100th-seconds byte is 00 or 99 the read is
 * repeated until two in a row agree (datasheet 4.5, hint), so a rollover cannot tear the value.
 * 2000..2099 only. */
int rv_read_ms(const struct rv_bus *b, uint64_t *unix_ms);

/* Sets the clock (Seconds..Year in one burst, which also zeroes the 100th seconds) and then clears
 * PORF and VLF. Resolution is 1 s: sub-second is dropped. */
int rv_set_ms(const struct rv_bus *b, uint64_t unix_ms);

/* Calendar <-> unix seconds (UTC, 2000..2099), exposed for tests. */
uint32_t rv_civil_to_unix(unsigned year, unsigned mon, unsigned day, unsigned h, unsigned m, unsigned s);
void rv_unix_to_civil(uint32_t t, unsigned *year, unsigned *mon, unsigned *day, unsigned *h, unsigned *m,
		      unsigned *s, unsigned *wday);

/* ---- GRTC / RTC correspondence (issue #9) ----
 * Records carry both the RTC time and a monotonic count (GRTC, per boot). Between two RTC reads the
 * RTC should advance as the monotonic count does, apart from crystal drift. A step bigger than that
 * means the RTC was set or lost power; the caller appends a correction record. */
enum tk_event {
	TK_OK,
	TK_JUMP_FWD,
	TK_JUMP_BACK,
	TK_INVALID, /* PORF / VLF: the time cannot be trusted; records get the RTC_INVALID flag */
};

struct tk {
	uint64_t last_rtc_ms;
	uint32_t last_mono_ms;
	uint8_t have;
};

/* delta_ms (optional) = observed - expected. Tolerance: 250 ms + 200 ppm of the elapsed time
 * (the LFXO that drives the monotonic count is not temperature compensated; 02: +-20 ppm). */
enum tk_event tk_observe(struct tk *k, uint64_t rtc_ms, uint32_t mono_ms, int rtc_valid, int64_t *delta_ms);

#endif
