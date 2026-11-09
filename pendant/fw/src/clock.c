#include <errno.h>
#include <zephyr/kernel.h>
#include "clock.h"

/* 2000-01-01 .. 2100-01-01 UTC. The phone sends the current time; reject 0 and absurd values
 * so a bad TIME cannot become a "valid" record timestamp. */
#define CLOCK_MIN_MS 946684800000ULL
#define CLOCK_MAX_MS 4102444800000ULL

static int64_t offset_ms; /* unix ms - uptime ms; valid when set */
static int is_set;
static K_MUTEX_DEFINE(clock_lock);

uint64_t clock_rtc_ms(void)
{
	uint64_t now;

	k_mutex_lock(&clock_lock, K_FOREVER);
	now = is_set ? (uint64_t)(offset_ms + k_uptime_get()) : 0;
	k_mutex_unlock(&clock_lock);
	return now;
}

int clock_time_valid(uint64_t unix_ms)
{
	return unix_ms >= CLOCK_MIN_MS && unix_ms <= CLOCK_MAX_MS;
}

int clock_set(uint64_t unix_ms)
{
	if (!clock_time_valid(unix_ms)) {
		return -EINVAL;
	}
	k_mutex_lock(&clock_lock, K_FOREVER);
	offset_ms = (int64_t)unix_ms - k_uptime_get();
	is_set = 1;
	k_mutex_unlock(&clock_lock);
	return 0;
}
