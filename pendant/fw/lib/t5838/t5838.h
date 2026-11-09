/* T5838 one-wire configuration on THSEL (DS-000383 Rev 1.0 §5.9-5.10) and CLK mode table. Pure C:
 * produces the pulse train as (level, CLK cycles) pairs, so any timer / PWM / bit-bang can play it.
 * Issue #10, docs/spec/audio.md §4. */
#ifndef T5838_H
#define T5838_H

#include <stddef.h>
#include <stdint.h>

struct t5838_pulse {
	uint8_t high;
	uint32_t clk; /* duration in CLK cycles */
};

#define T5838_DEV_ADDR_W 0xa6 /* 7'b1010011, then 0 for write */
#define T5838_MAX_PULSES (2 + 24 * 2 + 1)

/* START(pilot tp CLK, 8..20) + space + 24 bits (zero = 1 tp high, one = 3 tp high, each followed by
 * a 1 tp space) + STOP (high, 130 CLK; the datasheet wants > 128). tp is the pilot width in CLK
 * cycles. Returns the number of pulses written, 0 if tp is out of range or out is too small. */
size_t t5838_write_pulses(struct t5838_pulse *out, size_t max, uint8_t reg, uint8_t data, unsigned tp);

/* Checks a pulse train against the symbol limits of Table 12 and decodes it. 0 on success. */
int t5838_decode_pulses(const struct t5838_pulse *p, size_t n, uint8_t *reg, uint8_t *data, unsigned *tp);

struct t5838_reg {
	uint8_t reg, data;
};

/* The five writes that unlock the AAD registers (Table 14). Once per power-up. */
extern const struct t5838_reg t5838_unlock[5];

/* AAD Analog (the 20 uA mode): LPF code 1..7, threshold code from t5838_aad_a_code(). */
size_t t5838_aad_a_cfg(struct t5838_reg *out, uint8_t lpf, uint8_t th_code);
/* Lowest threshold code at or above the wanted level (60..97.5 dB SPL at 1 kHz); 0xff if above 97.5. */
uint8_t t5838_aad_a_code(unsigned db_spl_x10);

enum t5838_mode { T5838_SLEEP, T5838_AAD_WRITE, T5838_LOW_POWER, T5838_HIGH_QUALITY, T5838_ULTRASONIC, T5838_BAD };
/* Mode the microphone is in for a CLK frequency (Hz). 0 = off = sleep with AAD A / D2 running;
 * 50-200 kHz = sleep, one-wire writes possible; 400-800 kHz low power; 2.0-3.7 MHz high quality. */
enum t5838_mode t5838_mode_for_clk(uint32_t hz);

#endif
