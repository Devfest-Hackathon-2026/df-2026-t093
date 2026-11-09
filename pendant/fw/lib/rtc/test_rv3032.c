/* Host test: make -C fw/lib rtc. A register-level model of the RV-3032-C7 (BCD calendar at 00h..07h,
 * status 0Dh, EEPROM PMU C0h) including the torn read around a second boundary. */
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include "rv3032.h"

struct mock {
	uint8_t reg[256];
	int torn_once;  /* next burst read of the time returns a half-updated value */
	int reads, writes;
	int time_writes_split; /* a write that did not cover Seconds..Year in one burst */
};

static int m_read(void *c, uint8_t reg, uint8_t *buf, size_t n)
{
	struct mock *m = c;

	m->reads++;
	memcpy(buf, m->reg + reg, n);
	if (reg == 0 && n == 8 && m->torn_once) { /* seconds rolled over between two bytes */
		m->torn_once = 0;
		buf[0] = 0x00;                    /* new hundredths ... */
		buf[1] = (uint8_t)(buf[1] - 1);   /* ... but the old seconds */
	}
	return 0;
}

static int m_write(void *c, uint8_t reg, const uint8_t *buf, size_t n)
{
	struct mock *m = c;

	m->writes++;
	if (reg == 0x01 && n != 7) {
		m->time_writes_split = 1;
	}
	memcpy(m->reg + reg, buf, n);
	if (reg == 0x01) {
		m->reg[0x00] = 0; /* writing Seconds zeroes the 100th seconds */
	}
	return 0;
}

static void set_bcd_time(struct mock *m, uint8_t hs, uint8_t s, uint8_t mi, uint8_t h, uint8_t d, uint8_t mo, uint8_t y)
{
	m->reg[0] = hs;
	m->reg[1] = s;
	m->reg[2] = mi;
	m->reg[3] = h;
	m->reg[5] = d;
	m->reg[6] = mo;
	m->reg[7] = y;
}

int main(void)
{
	struct mock m;
	struct rv_bus b = {.ctx = &m, .read = m_read, .write = m_write};
	uint64_t ms;
	uint8_t st;

	/* 1. Calendar <-> unix, both ways, against reference values (and leap days). */
	{
		static const struct { uint32_t t; unsigned y, mo, d, h, mi, s, wd; } v[] = {
			{946684800, 2000, 1, 1, 0, 0, 0, 6},        {1709251199, 2024, 2, 29, 23, 59, 59, 4},
			{1791948615, 2026, 10, 14, 3, 30, 15, 3},   {2147483648u, 2038, 1, 19, 3, 14, 8, 2},
			{4102444799u, 2099, 12, 31, 23, 59, 59, 4},
		};

		for (unsigned i = 0; i < sizeof(v) / sizeof(v[0]); i++) {
			unsigned y, mo, d, h, mi, s, wd;

			assert(rv_civil_to_unix(v[i].y, v[i].mo, v[i].d, v[i].h, v[i].mi, v[i].s) == v[i].t);
			rv_unix_to_civil(v[i].t, &y, &mo, &d, &h, &mi, &s, &wd);
			assert(y == v[i].y && mo == v[i].mo && d == v[i].d && h == v[i].h && mi == v[i].mi && s == v[i].s &&
			       wd == v[i].wd);
		}
		for (uint32_t t = 946684800; t < 4102444800u; t += 86399 * 7 + 13) { /* round trip across the whole range */
			unsigned y, mo, d, h, mi, s, wd;

			rv_unix_to_civil(t, &y, &mo, &d, &h, &mi, &s, &wd);
			assert(rv_civil_to_unix(y, mo, d, h, mi, s) == t);
		}
	}

	/* 2. Config check: delivery default is fine; switchover or trickle charging is refused (docs/spec/firmware.md §5.3). */
	memset(&m, 0, sizeof(m));
	assert(rv_config_ok(&b) == 0);
	m.reg[0xc0] = 0x30; /* BSM = 11: disabled, fine */
	assert(rv_config_ok(&b) == 0);
	m.reg[0xc0] = 0x10; /* BSM = 01: direct switching mode */
	assert(rv_config_ok(&b) == -EPERM);
	m.reg[0xc0] = 0x20; /* BSM = 10: level switching */
	assert(rv_config_ok(&b) == -EPERM);
	m.reg[0xc0] = 0x01; /* TCM != 0: trickle charger on */
	assert(rv_config_ok(&b) == -EPERM);

	/* 3. Time read: BCD, hundredths, year 2000 base. */
	memset(&m, 0, sizeof(m));
	set_bcd_time(&m, 0x42, 0x15, 0x30, 0x03, 0x14, 0x10, 0x26); /* 2026-10-14 03:30:15.42 */
	assert(!rv_read_ms(&b, &ms) && ms == 1791948615420ULL);

	/* 4. A read right on the 100th = 00 boundary that tears: the repeat rule returns a consistent value. */
	set_bcd_time(&m, 0x00, 0x16, 0x30, 0x03, 0x14, 0x10, 0x26);
	m.torn_once = 1;
	assert(!rv_read_ms(&b, &ms) && ms == 1791948616000ULL); /* never the torn :15.00 */
	set_bcd_time(&m, 0x99, 0x15, 0x30, 0x03, 0x14, 0x10, 0x26);
	m.reads = 0;
	assert(!rv_read_ms(&b, &ms) && ms == 1791948615990ULL && m.reads == 2); /* 99 also reads twice */
	set_bcd_time(&m, 0x50, 0x15, 0x30, 0x03, 0x14, 0x10, 0x26);
	m.reads = 0;
	assert(!rv_read_ms(&b, &ms) && m.reads == 1); /* mid-second: a single read */

	/* 5. Status flags are visible and nothing clears them behind our back (read, config, time reads). */
	m.reg[0x0d] = RV_STATUS_PORF | RV_STATUS_VLF;
	assert(!rv_config_ok(&b) && !rv_read_ms(&b, &ms) && !rv_status(&b, &st) && st == 3);

	/* 6. Setting the time: one Seconds..Year burst, 100th seconds zeroed, flags cleared afterwards. */
	m.writes = 0;
	m.time_writes_split = 0;
	assert(!rv_set_ms(&b, 1791948615999ULL)); /* sub-second is dropped */
	assert(!m.time_writes_split && m.writes == 2);
	assert(m.reg[1] == 0x15 && m.reg[2] == 0x30 && m.reg[3] == 0x03 && m.reg[4] == 3 && m.reg[5] == 0x14 &&
	       m.reg[6] == 0x10 && m.reg[7] == 0x26 && m.reg[0] == 0);
	assert(!rv_status(&b, &st) && st == 0);
	assert(!rv_read_ms(&b, &ms) && ms == 1791948615000ULL);
	assert(rv_set_ms(&b, 1000) == -ERANGE && rv_set_ms(&b, 4102444800000ULL) == -ERANGE);

	/* 7. GRTC / RTC watcher: steady, drift within tolerance, jumps both ways, invalid, recovery. */
	{
		struct tk k = {0};
		int64_t d;

		assert(tk_observe(&k, 1000000, 5000, 1, &d) == TK_OK);                         /* first reading: nothing to compare */
		assert(tk_observe(&k, 1008000, 13000, 1, &d) == TK_OK && d == 0);              /* 8 s, in step */
		assert(tk_observe(&k, 1016050, 21000, 1, &d) == TK_OK && d == 50);             /* 50 ms of jitter */
		assert(tk_observe(&k, 1016050 + 7200000 + 1000, 21000 + 7200000, 1, &d) == TK_OK && d == 1000); /* 2 h, 139 ppm drift */
		assert(tk_observe(&k, 2000000, 7240000, 1, &d) == TK_JUMP_BACK && d < -5000000); /* RTC set far back */
		assert(tk_observe(&k, 2008000, 7248000, 1, &d) == TK_OK && d == 0);            /* and tracked again */
		assert(tk_observe(&k, 9000000, 7256000, 1, &d) == TK_JUMP_FWD && d > 6000000);
		assert(tk_observe(&k, 0, 7264000, 0, &d) == TK_INVALID && !k.have);            /* PORF / VLF */
		assert(tk_observe(&k, 9500000, 7272000, 1, &d) == TK_OK && k.have);            /* first after invalid: nothing to compare */
	}

	puts("rv3032: all tests passed");
	return 0;
}
