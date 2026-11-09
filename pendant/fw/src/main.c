#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include "audio.h"
#include "ble.h"
#include "marker.h"
#include "scenario.h"
#include "ota.h"
#include "store.h"
#include "supervisor.h"

LOG_MODULE_REGISTER(pendant, LOG_LEVEL_INF);

#ifdef CONFIG_PENDANT_CRYPTO_BENCH
void crypto_bench(void);
#endif

static uint32_t audio_next_try;
static int audio_fails, audio_retry_pending;

static void maybe_start_audio(int store_ok, int wdt_ok)
{
	uint32_t now, delay;
	int err;

	if (!IS_ENABLED(CONFIG_PENDANT_AUTO_RECORD) || !store_ok || !wdt_ok || audio_running() ||
	    audio_faulted()) {
		return;
	}
	now = k_uptime_get_32();
	if (audio_retry_pending && (int32_t)(now - audio_next_try) < 0) {
		return;
	}
	err = audio_start();
	if (!err) {
		audio_fails = 0;
		audio_retry_pending = 0;
		LOG_INF("recording started");
		return;
	}
	audio_fails++;
	delay = MIN(30000u, 1000u << MIN(audio_fails, 5));
	audio_next_try = now + delay;
	audio_retry_pending = 1;
	LOG_ERR("record start failed (%d), retry in %u ms", err, delay);
}

int main(void)
{
#ifdef CONFIG_PENDANT_CRYPTO_BENCH
	crypto_bench();
#endif

	marker_init();
	uint8_t cause = supervisor_reset_cause();

	int wdt_ok = supervisor_init() == 0;
	int store_ok;

	ota_init();
	store_ok = store_init() == 0;
	if (!store_ok) {
		LOG_ERR("store_init failed; continuing without recording");
	}
	if (store_ok) {
		uint8_t ev[CHUNK_EVENT_SIZE];

		chunk_event_encode_arg(ev, CHUNK_EV_RESET, cause, k_uptime_get_32());
		LOG_INF("reset event record: %d", store_append(CHUNK_EVENT, ev, sizeof(ev)));
	}
	if (IS_ENABLED(CONFIG_PENDANT_MEASURE) && store_ok && wdt_ok) {
		ble_enable();
		scenario_run();
	}
	if (IS_ENABLED(CONFIG_PENDANT_BLE)) {
		LOG_INF("ble init: %d", ble_init());
	}
	maybe_start_audio(store_ok, wdt_ok);
	for (;;) {
		const struct audio_stats *a = audio_stats();
		const struct store_stats *s = store_stats();
		struct ring *r = store_ring();

		k_sleep(K_SECONDS(5));
		sup_beat(SUP_MAIN);
		maybe_start_audio(store_ok, wdt_ok);
		if (wdt_ok && store_ok) {
			ota_poll();
		}
		LOG_INF("store %d wdt %d | blocks %u chunks %u rderr %u nobuf %u | wr %u err %u last %u max %u ms | "
			"tail %u next %u | mic min %d max %d ms2 %u",
			store_ok, wdt_ok, a->blocks, a->chunks, a->read_errors, a->no_buffer, s->chunks_written,
			s->write_errors, s->last_write_ms, s->max_write_ms, r->tail_seq, r->next_seq, a->last_min,
			a->last_max, a->last_rms2);
	}
	return 0;
}
