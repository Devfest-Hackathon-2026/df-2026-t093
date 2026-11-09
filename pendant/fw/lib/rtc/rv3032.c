#include <errno.h>
#include <string.h>
#include "rv3032.h"

#define REG_TIME 0x00 /* 100th s, s, min, h, weekday, date, month, year (BCD) */
#define REG_STATUS 0x0d
#define REG_PMU 0xc0

static uint8_t bcd(unsigned v)
{
	return (uint8_t)((v / 10) << 4 | v % 10);
}

static unsigned unbcd(uint8_t v)
{
	return (v >> 4) * 10 + (v & 15);
}

/* days since 1970-01-01 (Howard Hinnant's civil algorithm) */
static int32_t days_from_civil(int y, unsigned m, unsigned d)
{
	y -= m <= 2;
	int era = (y >= 0 ? y : y - 399) / 400;
	unsigned yoe = (unsigned)(y - era * 400);
	unsigned doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
	unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;

	return era * 146097 + (int32_t)doe - 719468;
}

uint32_t rv_civil_to_unix(unsigned year, unsigned mon, unsigned day, unsigned h, unsigned m, unsigned s)
{
	return (uint32_t)days_from_civil((int)year, mon, day) * 86400u + h * 3600u + m * 60u + s;
}

void rv_unix_to_civil(uint32_t t, unsigned *year, unsigned *mon, unsigned *day, unsigned *h, unsigned *m,
		      unsigned *s, unsigned *wday)
{
	uint32_t days = t / 86400u, rem = t % 86400u;
	int32_t z = (int32_t)days + 719468;
	int era = z / 146097;
	unsigned doe = (unsigned)(z - era * 146097);
	unsigned yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
	int y = (int)yoe + era * 400;
	unsigned doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
	unsigned mp = (5 * doy + 2) / 153;
	unsigned d = doy - (153 * mp + 2) / 5 + 1;
	unsigned mo = mp < 10 ? mp + 3 : mp - 9;

	*year = (unsigned)(y + (mo <= 2));
	*mon = mo;
	*day = d;
	*h = rem / 3600;
	*m = rem % 3600 / 60;
	*s = rem % 60;
	*wday = (days + 4) % 7; /* 1970-01-01 was a Thursday; Sunday = 0 */
}

int rv_status(const struct rv_bus *b, uint8_t *status)
{
	return b->read(b->ctx, REG_STATUS, status, 1);
}

int rv_config_ok(const struct rv_bus *b)
{
	uint8_t pmu;
	int err = b->read(b->ctx, REG_PMU, &pmu, 1);

	if (err) {
		return err;
	}
	/* BSM (bits 5:4) 00 or 11 = switchover disabled; TCM (1:0) 00 = trickle charger off. */
	unsigned bsm = pmu >> 4 & 3;

	return (bsm == 0 || bsm == 3) && (pmu & 3) == 0 ? 0 : -EPERM;
}

int rv_read_ms(const struct rv_bus *b, uint64_t *unix_ms)
{
	uint8_t a[8], c[8];
	int err = b->read(b->ctx, REG_TIME, a, 8);

	for (int tries = 0; !err && (a[0] == 0x00 || a[0] == 0x99) && tries < 4; tries++) {
		err = b->read(b->ctx, REG_TIME, c, 8); /* near a second boundary: read until two agree */
		if (err) {
			break;
		}
		if (!memcmp(a, c, 8)) {
			break;
		}
		memcpy(a, c, 8);
	}
	if (err) {
		return err;
	}
	uint32_t t = rv_civil_to_unix(2000 + unbcd(a[7]), unbcd(a[6] & 0x1f), unbcd(a[5] & 0x3f), unbcd(a[3] & 0x3f),
				      unbcd(a[2] & 0x7f), unbcd(a[1] & 0x7f));

	*unix_ms = (uint64_t)t * 1000 + unbcd(a[0]) * 10;
	return 0;
}

int rv_set_ms(const struct rv_bus *b, uint64_t unix_ms)
{
	unsigned y, mo, d, h, mi, s, wd;
	uint8_t t[7], st = 0;
	int err;

	if (unix_ms < 946684800000ULL || unix_ms >= 4102444800000ULL) {
		return -ERANGE;
	}
	rv_unix_to_civil((uint32_t)(unix_ms / 1000), &y, &mo, &d, &h, &mi, &s, &wd);
	t[0] = bcd(s);
	t[1] = bcd(mi);
	t[2] = bcd(h);
	t[3] = (uint8_t)wd;
	t[4] = bcd(d);
	t[5] = bcd(mo);
	t[6] = bcd(y - 2000);
	err = b->write(b->ctx, REG_TIME + 1, t, 7); /* one burst, Seconds through Year */
	return err ? err : b->write(b->ctx, REG_STATUS, &st, 1); /* the time is good again */
}

enum tk_event tk_observe(struct tk *k, uint64_t rtc_ms, uint32_t mono_ms, int rtc_valid, int64_t *delta_ms)
{
	int64_t d = 0;
	enum tk_event ev = TK_OK;

	if (!rtc_valid) {
		k->have = 0; /* no reference to compare with until the time is set again */
		ev = TK_INVALID;
	} else if (k->have) {
		uint32_t el = mono_ms - k->last_mono_ms;
		int64_t expected = (int64_t)k->last_rtc_ms + el, tol = 250 + el / 5000;

		d = (int64_t)rtc_ms - expected;
		ev = d > tol ? TK_JUMP_FWD : d < -tol ? TK_JUMP_BACK : TK_OK;
	}
	if (rtc_valid) {
		k->last_rtc_ms = rtc_ms;
		k->last_mono_ms = mono_ms;
		k->have = 1;
	}
	if (delta_ms) {
		*delta_ms = d;
	}
	return ev;
}
