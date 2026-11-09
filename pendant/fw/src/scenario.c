/* Measurement scenario (docs/plan/prototype.md §2.4): the five states whose difference currents we want, one after
 * another, 20 s each, announced by n pulses on the BLE marker so the scope can find them. */
#include <zephyr/kernel.h>
#include "audio.h"
#include "ble.h"
#include "marker.h"
#include "scenario.h"
#include "store.h"
#include "supervisor.h"
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(scenario, LOG_LEVEL_INF);

#define SEG_S 20

static void sleep_beat(uint32_t ms)
{
	while (ms) {
		uint32_t step = MIN(ms, 1000);

		k_msleep(step);
		sup_beat(SUP_MAIN);
		ms -= step;
	}
}

static void announce(int n)
{
	const struct audio_stats *a = audio_stats();

	sup_beat(SUP_MAIN);

	LOG_INF("segment %d (blocks %u, chunks %u, written %u)", n, a->blocks, a->chunks, store_stats()->chunks_written);
	for (int i = 0; i < n; i++) {
		marker_set(M_BLE, 1);
		k_busy_wait(2000);
		marker_set(M_BLE, 0);
		k_busy_wait(2000);
	}
	k_msleep(100);
}

#if CONFIG_PENDANT_TEST_AUDIO_CYCLES
static void lifecycle_test(void)
{
	const uint32_t cycle_ms = MAX(5000, CONFIG_PENDANT_CHUNK_BYTES / 4 + 1000);

	for (int i = 0; i < CONFIG_PENDANT_TEST_AUDIO_CYCLES; i++) {
		int err;

		audio_set_mode(AUDIO_FULL);
		err = audio_start();
		if (err) {
			LOG_ERR("lifecycle %d: start failed (%d)", i, err);
			return;
		}
		sleep_beat(cycle_ms); /* at least one full chunk plus a partial one */
		audio_stop();
		if (audio_running()) {
			LOG_ERR("lifecycle %d: thread still running", i);
			return;
		}
		if (!audio_stats()->chunks) {
			LOG_ERR("lifecycle %d: no chunk was produced", i);
			return;
		}
		if (!CONFIG_PENDANT_TEST_WRITER_STALL_MS && audio_stats()->no_buffer) {
			LOG_ERR("lifecycle %d: lost a buffer (no_buffer %u)", i, audio_stats()->no_buffer);
			return;
		}
	}
	LOG_INF("lifecycle: %d start/stop cycles passed", CONFIG_PENDANT_TEST_AUDIO_CYCLES);
}
#endif

void scenario_run(void)
{
#if CONFIG_PENDANT_TEST_AUDIO_CYCLES
	lifecycle_test();
#endif
	for (;;) {
		int err;

		announce(1); /* 1: idle: the core sleeps, mic off, radio off */
		sleep_beat(SEG_S * 1000);

		announce(2); /* 2: PDM + DMA only */
		audio_set_mode(AUDIO_PDM_ONLY);
		err = audio_start();

		if (err) {
			LOG_ERR("segment 2 start failed (%d)", err);
		}
		sleep_beat(SEG_S * 1000);
		audio_stop();

		announce(3); /* 3: PDM + ADPCM, no flash writes */
		audio_set_mode(AUDIO_ENCODE);
		err = audio_start();

		if (err) {
			LOG_ERR("segment 3 start failed (%d)", err);
		}
		sleep_beat(SEG_S * 1000);
		audio_stop();

		announce(4); /* 4: PDM + ADPCM + ring writes (the normal recording) */
		audio_set_mode(AUDIO_FULL);
		err = audio_start();

		if (err) {
			LOG_ERR("segment 4 start failed (%d)", err);
		}
		sleep_beat(SEG_S * 1000);
		audio_stop();

#ifdef CONFIG_BT
		announce(5); /* 5: advertising, connectable, no connection */
		ble_adv_start();
		sleep_beat(SEG_S * 1000);
		ble_adv_stop();
#endif
	}
}
