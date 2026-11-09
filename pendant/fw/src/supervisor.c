#include <errno.h>
#include <zephyr/drivers/hwinfo.h>
#include <zephyr/drivers/watchdog.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include "chunk.h"
#include "health.h"
#include "supervisor.h"

LOG_MODULE_REGISTER(sup, LOG_LEVEL_INF);

static struct health health;
static const struct device *const wdt = DEVICE_DT_GET(DT_ALIAS(watchdog0));
static int wdt_ch = -1;

uint8_t supervisor_reset_cause(void)
{
	uint32_t c = 0;
	uint8_t r = CHUNK_RESET_OTHER;

	hwinfo_get_reset_cause(&c);
	hwinfo_clear_reset_cause();
	if (c & RESET_WATCHDOG) {
		r = CHUNK_RESET_WATCHDOG;
	} else if (c & RESET_BROWNOUT) {
		r = CHUNK_RESET_BROWNOUT;
	} else if (c & RESET_POR) {
		r = CHUNK_RESET_POR;
	} else if (c & RESET_SOFTWARE) {
		r = CHUNK_RESET_SOFT;
	} else if (c & RESET_PIN) {
		r = CHUNK_RESET_PIN;
	}
	LOG_INF("reset cause mask 0x%x -> %u", c, r);
	return r;
}

void sup_arm(enum sup_task t, uint32_t timeout_ms)
{
	health_arm(&health, t, timeout_ms, k_uptime_get_32());
}

void sup_disarm(enum sup_task t)
{
	health_disarm(&health, t);
}

void sup_beat(enum sup_task t)
{
	health_beat(&health, t, k_uptime_get_32());
}

static void sup_thread(void *a, void *b, void *c)
{
	for (;;) {
		k_sleep(K_SECONDS(1));
		if (health_ok(&health, k_uptime_get_32())) {
			wdt_feed(wdt, wdt_ch);
		} else {
			LOG_ERR("not feeding the watchdog: stuck mask 0x%x", health.stuck);
		}
	}
}

K_THREAD_DEFINE(sup_tid, 1024, sup_thread, NULL, NULL, NULL, 2, 0, K_TICKS_FOREVER);

int supervisor_init(void)
{
	struct wdt_timeout_cfg cfg = {.window = {.min = 0, .max = CONFIG_PENDANT_WDT_TIMEOUT_MS},
				      .flags = WDT_FLAG_RESET_SOC};

	health_init(&health);
	sup_arm(SUP_MAIN, 20000);
	if (!IS_ENABLED(CONFIG_PENDANT_WDT)) {
		return 0; /* development build; PENDANT_RELEASE rejects this */
	}
	if (!device_is_ready(wdt)) {
		LOG_ERR("watchdog device not ready");
		return -ENODEV;
	}
	wdt_ch = wdt_install_timeout(wdt, &cfg);
	if (wdt_ch < 0) {
		LOG_ERR("watchdog install failed: %d", wdt_ch);
		return -EIO;
	}
	int err = wdt_setup(wdt, WDT_OPT_PAUSE_HALTED_BY_DBG);

	if (err) {
		LOG_ERR("watchdog setup failed: %d", err);
		return -EIO;
	}
	k_thread_start(sup_tid);
	LOG_INF("watchdog %d ms, supervisor running", CONFIG_PENDANT_WDT_TIMEOUT_MS);
	return 0;
}
