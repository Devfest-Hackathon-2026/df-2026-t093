#include <errno.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include "audio.h"

#if defined(CONFIG_PENDANT_AUDIO_SOURCE_PDM)
#include <zephyr/audio/dmic.h>
#include <zephyr/drivers/regulator.h>
#include <hal/nrf_pdm.h>
#elif defined(CONFIG_PENDANT_AUDIO_SOURCE_I2S)
#include <zephyr/drivers/i2s.h>
#include <zephyr/drivers/regulator.h>
#endif

#if !defined(CONFIG_PENDANT_AUDIO_SOURCE_NONE)
#include "audio_dsp.h"
#include "clock.h"
#include "ima_adpcm.h"
#include "marker.h"
#include "store.h"
#include "supervisor.h"
#endif

LOG_MODULE_REGISTER(audio, LOG_LEVEL_INF);

static struct audio_stats stats;
static volatile enum audio_mode mode = AUDIO_FULL;
static atomic_t running = ATOMIC_INIT(0);
static volatile int faulted;

void audio_set_mode(enum audio_mode m)
{
	mode = m;
}

int audio_running(void)
{
	return atomic_get(&running);
}

int audio_faulted(void)
{
	return faulted;
}

const struct audio_stats *audio_stats(void)
{
	return &stats;
}

#if defined(CONFIG_PENDANT_AUDIO_SOURCE_PDM) || defined(CONFIG_PENDANT_AUDIO_SOURCE_I2S)

#define RATE      CONFIG_PENDANT_PCM_RATE
#define BLK_SMP   (RATE / 10) /* 100 ms */
#define BLK_BYTES (BLK_SMP * 2)
#define ENC_BYTES (BLK_SMP / 2)

BUILD_ASSERT(CONFIG_PENDANT_CHUNK_BYTES % ENC_BYTES == 0, "chunk must be whole 100 ms blocks");

#if defined(CONFIG_PENDANT_AUDIO_SOURCE_PDM)
K_MEM_SLAB_DEFINE_STATIC(slab, BLK_BYTES, 8, 4);
#else
K_MEM_SLAB_DEFINE_STATIC(slab, BLK_BYTES, 2, 4);
#endif

static volatile int run, active, thread_started;
static struct chunk_buf *volatile active_cb;
static uint32_t injected_read_errors;
static K_SEM_DEFINE(stopped, 0, 1);

static void thread_fn(void *a, void *b, void *c);
K_THREAD_STACK_DEFINE(stack, 3072);
static struct k_thread thread;

struct audio_ctx {
	struct ima_adpcm st;
	struct chunk_buf *cb;
	uint32_t off;
	struct audio_dsp_dc dc;
	uint8_t no_buffer_streak;
};

static void process_block(struct audio_ctx *c, int16_t *s, size_t n)
{
	if (store_halted()) {
		LOG_ERR("storage halted; stopping audio");
		faulted = 1;
		run = 0;
		return;
	}
	if (mode == AUDIO_PDM_ONLY) { /* source and DMA only */
		stats.blocks++;
		return;
	}
	if (!c->cb) { /* a new record starts with this block */
		c->cb = store_buf_get(K_NO_WAIT);
		if (!c->cb) {
			stats.no_buffer++; /* writer still busy with both buffers: drop the block */
			store_note_drop(1);
			if (++c->no_buffer_streak >= 100) {
				LOG_ERR("no chunk buffer for 10 s; stopping audio");
				faulted = 1;
				run = 0;
			}
			return;
		}
		c->no_buffer_streak = 0;
		active_cb = c->cb;
		ima_adpcm_reset(&c->st); /* every record decodes on its own (docs/spec/formats.md §2) */
		c->off = 0;
		uint32_t mono = k_uptime_get_32();

		c->cb->mono_ms = mono > 100 ? mono - 100 : 0; /* the block just ended */
		uint64_t rtc = clock_rtc_ms();

		if (rtc >= 100) {
			c->cb->rtc_ms = rtc - 100;
			c->cb->flags = 0;
		} else {
			c->cb->rtc_ms = 0;
			c->cb->flags = CHUNK_F_RTC_INVALID;
		}
	}
	{
		int32_t mn = 32767, mx = -32768;
		int64_t sq = 0;

		for (size_t i = 0; i < n; i++) {
			mn = MIN(mn, s[i]);
			mx = MAX(mx, s[i]);
			sq += (int32_t)s[i] * s[i];
		}
		stats.last_min = mn;
		stats.last_max = mx;
		stats.last_rms2 = sq / n;
	}
	if (IS_ENABLED(CONFIG_PENDANT_DC_BLOCK)) {
		audio_dsp_dc_block(&c->dc, s, n);
	}
	ima_adpcm_encode(&c->st, s, n, c->cb->payload + c->off);
	stats.blocks++;
	c->off += ENC_BYTES;
	if (c->off == CONFIG_PENDANT_CHUNK_BYTES) {
		struct chunk_buf *done = c->cb;

		done->len = c->off;
		c->cb = NULL;
		active_cb = NULL; /* the queue owns it from here; stop must not release it */
		if (mode == AUDIO_FULL) {
			int err;

			do {
				err = store_buf_submit_timeout(done, K_MSEC(100));
			} while (err && run);
			if (err) { /* only when the writer is still stalled and stop was requested */
				store_buf_release(done);
			} else {
				stats.chunks++;
			}
		} else {
			store_buf_release(done); /* ENCODE only: nothing is written */
			stats.chunks++;
		}
	}
}

static void thread_exit(struct audio_ctx *c)
{
	if (faulted) {
		store_flush_drops(); /* best effort before the watchdog reset */
	}
	active_cb = NULL; /* clear before releasing: the stop-timeout abort path must not release it again */
	if (c->cb) {
		store_buf_release(c->cb);
		c->cb = NULL;
	}
	atomic_set(&running, 0);
	k_sem_give(&stopped);
}

static bool stop_thread(void)
{
	run = 0;
	if (thread_started) {
		int err = k_sem_take(&stopped, K_SECONDS(3));

		if (err) { /* only a stuck driver/test hang should get here */
			LOG_ERR("audio thread did not stop (%d); aborting", err);
			k_thread_abort(&thread);
		}
		if (k_thread_join(&thread, K_SECONDS(1)) != 0) {
			LOG_ERR("audio thread did not join; refusing to reuse its stack");
			return false; /* keep thread_started set: the next start fails instead of reusing it */
		}
		thread_started = 0;
		if (active_cb) { /* the thread is dead now; release only after join */
			store_buf_release((struct chunk_buf *)active_cb);
			active_cb = NULL;
		}
	}
	return true;
}

#if defined(CONFIG_PENDANT_AUDIO_SOURCE_PDM)

static const struct device *const mic_pwr = DEVICE_DT_GET(DT_NODELABEL(pdm_imu_pwr));
static const struct device *const dmic = DEVICE_DT_GET(DT_NODELABEL(pdm20));

static void thread_fn(void *a, void *b, void *c)
{
	struct audio_ctx ctx = {0};
	int read_errors = 0;

	atomic_set(&running, 1);
	while (run) {
		void *blk;
		size_t sz;
		int err;

		if (CONFIG_PENDANT_TEST_READ_ERRORS &&
		    injected_read_errors < CONFIG_PENDANT_TEST_READ_ERRORS) {
			err = -EIO;
			injected_read_errors++;
		} else {
			err = dmic_read(dmic, 0, &blk, &sz, 200);
		}

		if (CONFIG_PENDANT_TEST_HANG_S && stats.blocks > CONFIG_PENDANT_TEST_HANG_S * 10) {
			LOG_ERR("test: audio thread hangs now");
			for (;;) {
				k_busy_wait(1000000);
			}
		}

		if (err == -EAGAIN) { /* no block yet; the read timeout is shorter than a block */
			continue;
		}
		if (err) {
			stats.read_errors++;
			read_errors++;
			if (read_errors >= CONFIG_PENDANT_AUDIO_MAX_READ_ERRORS) {
				LOG_ERR("audio read failed %d times (%d); stopping", read_errors, err);
				faulted = 1;
				run = 0; /* leave the supervisor armed: a dead mic must reset, not feed the WDT */
				break;
			}
			continue;
		}
		if (sz != BLK_BYTES) {
			stats.read_errors++;
			store_note_drop(1);
			read_errors++;
			k_mem_slab_free(&slab, blk);
			if (read_errors >= CONFIG_PENDANT_AUDIO_MAX_READ_ERRORS) {
				LOG_ERR("PDM returned a short block %u times; stopping", read_errors);
				faulted = 1;
				run = 0;
				break;
			}
			continue;
		}
		read_errors = 0;
		process_block(&ctx, blk, BLK_SMP);
		sup_beat(SUP_AUDIO);
		k_mem_slab_free(&slab, blk);
	}
	thread_exit(&ctx);
}

int audio_start(void)
{
	struct pcm_stream_cfg stream = {.pcm_width = 16, .mem_slab = &slab, .pcm_rate = CONFIG_PENDANT_PCM_RATE,
					.block_size = BLK_BYTES};
	struct dmic_cfg cfg = {
		.io = {.min_pdm_clk_freq = CONFIG_PENDANT_PDM_CLK_MIN, .max_pdm_clk_freq = CONFIG_PENDANT_PDM_CLK_MAX,
		       .min_pdm_clk_dc = 40, .max_pdm_clk_dc = 60},
		.streams = &stream,
		.channel = {.req_num_streams = 1, .req_num_chan = 1,
			    .req_chan_map_lo = dmic_build_channel_map(0, 0, PDM_CHAN_LEFT)},
	};
	int err;

	if (run || active || thread_started) {
		return -EALREADY;
	}
	stats = (struct audio_stats){0};
	faulted = 0;
	k_sem_reset(&stopped);
	err = regulator_enable(mic_pwr); /* off whenever not recording: no floor current */
	if (!err) {
		for (int i = 0; i < 10; i++) { /* STOP is acknowledged in the driver's ISR */
			err = dmic_configure(dmic, &cfg);
			if (err != -EBUSY) {
				break;
			}
			k_msleep(1);
		}
	}
	if (!err) { /* the Zephyr driver leaves the hardware gain at 0 dB */
		nrf_pdm_gain_set(NRF_PDM20, CONFIG_PENDANT_MIC_GAIN, CONFIG_PENDANT_MIC_GAIN);
	}
	err = err ? err : dmic_trigger(dmic, DMIC_TRIGGER_START);
	if (err) {
		regulator_disable(mic_pwr);
		return err;
	}
	active = 1;
	run = 1;
	k_thread_create(&thread, stack, K_THREAD_STACK_SIZEOF(stack), thread_fn, NULL, NULL, NULL, 5, 0,
			K_NO_WAIT);
	thread_started = 1;
	sup_arm(SUP_AUDIO, 2000);
	marker_set(M_REC, 1);
	return 0;
}

void audio_stop(void)
{
	if (!stop_thread()) {
		return;
	}
	if (active) {
		(void)dmic_trigger(dmic, DMIC_TRIGGER_STOP);
		k_msleep(1); /* let the driver's STOPPED ISR clear active before the next configure */
		(void)regulator_disable(mic_pwr);
		active = 0;
	}
	sup_disarm(SUP_AUDIO);
	store_flush_drops();
	marker_set(M_REC, 0);
	atomic_set(&running, 0);
	faulted = 0;
}

#elif defined(CONFIG_PENDANT_AUDIO_SOURCE_I2S)

#define I2S_BLOCK_FRAMES (BLK_SMP * 2)
#define I2S_BLOCK_BYTES  (I2S_BLOCK_FRAMES * 2 * sizeof(uint32_t))

BUILD_ASSERT(CONFIG_PENDANT_PCM_RATE == AUDIO_DSP_OUT_RATE, "I2S path is fixed at 8 kHz output");

/* The nRF I2S driver keeps the current and next RX buffers in flight and allocates a
 * third one when a block completes, before the app can free the queued block. Two
 * blocks therefore stop the stream with an overrun; keep four (two spare for the app).
 * nRF54L15 has only a 32 MHz I2S source. With 24-bit words and 48X, nrfx picks
 * 32MDIV42 (LRCK ~15.873 kHz), so the output runs ~0.8% slow. #35 stays open until
 * the actual SCK is measured and the clock/compensation decision is made. */
K_MEM_SLAB_DEFINE_STATIC(i2s_slab, I2S_BLOCK_BYTES, 4, 4);

static const struct device *const mic_pwr = DEVICE_DT_GET(DT_NODELABEL(mic_en));
static const struct device *const i2s = DEVICE_DT_GET(DT_NODELABEL(i2s20));
static const struct i2s_config i2s_cfg = {
	.word_size = 24,
	.channels = 2,
	.format = I2S_FMT_DATA_FORMAT_I2S | I2S_FMT_DATA_ORDER_MSB,
	.options = I2S_OPT_BIT_CLK_CONT | I2S_OPT_BIT_CLK_CONTROLLER | I2S_OPT_FRAME_CLK_CONTROLLER,
	.frame_clk_freq = AUDIO_DSP_IN_RATE,
	.mem_slab = &i2s_slab,
	.block_size = I2S_BLOCK_BYTES,
	.timeout = 200,
};

static int i2s_reset(void)
{
	int err = i2s_trigger(i2s, I2S_DIR_RX, I2S_TRIGGER_DROP);

	if (!err) {
		k_msleep(1); /* let the STOPPED ISR release the old RX buffers */
	}
	return err;
}

static int i2s_start_rx(void)
{
	int err = i2s_trigger(i2s, I2S_DIR_RX, I2S_TRIGGER_START);

	for (int i = 0; err && i < 10; i++) {
		k_msleep(1); /* the overrun path stops asynchronously; START may be -EALREADY */
		err = i2s_trigger(i2s, I2S_DIR_RX, I2S_TRIGGER_START);
	}
	return err;
}

static int i2s_recover(void)
{
	int err = i2s_trigger(i2s, I2S_DIR_RX, I2S_TRIGGER_PREPARE);

	if (err) {
		/* PREPARE is only valid in ERROR. If the driver is already READY
		 * (for example after a failed START), a single START is worth trying;
		 * if it is RUNNING, START returns -EALREADY immediately. */
		return i2s_trigger(i2s, I2S_DIR_RX, I2S_TRIGGER_START);
	}
	err = i2s_start_rx();
	if (!err) {
		return 0;
	}
	if (i2s_reset()) {
		return err;
	}
	if (i2s_configure(i2s, I2S_DIR_RX, &i2s_cfg)) {
		return err;
	}
	return i2s_start_rx();
}

static void thread_fn(void *a, void *b, void *c)
{
	struct audio_ctx ctx = {0};
	struct audio_dsp_decim dec;
	int read_errors = 0;

	/* FIR/DC state stays continuous across records; only ADPCM restarts per record. */
	audio_dsp_decim_init(&dec);
	atomic_set(&running, 1);
	while (run) {
		void *in;
		size_t sz;
		int err;

		if (CONFIG_PENDANT_TEST_READ_ERRORS &&
		    injected_read_errors < CONFIG_PENDANT_TEST_READ_ERRORS) {
			err = -EIO;
			injected_read_errors++;
		} else {
			err = i2s_read(i2s, &in, &sz);
		}

		if (CONFIG_PENDANT_TEST_HANG_S && stats.blocks > CONFIG_PENDANT_TEST_HANG_S * 10) {
			LOG_ERR("test: audio thread hangs now");
			for (;;) {
				k_busy_wait(1000000);
			}
		}

		if (err == -EAGAIN) { /* no block yet; the read timeout is shorter than a block */
			continue;
		}
		if (err) {
			stats.read_errors++;
			if (err == -EIO && run && i2s_recover() == 0) {
				read_errors = 0; /* a successful re-arm is not a dead microphone */
				continue;
			}
			read_errors++;
			if (read_errors >= CONFIG_PENDANT_AUDIO_MAX_READ_ERRORS) {
				LOG_ERR("audio read failed %d times (%d); stopping", read_errors, err);
				faulted = 1;
				run = 0;
				break;
			}
			continue;
		}
		if (sz != I2S_BLOCK_BYTES) {
			stats.read_errors++;
			store_note_drop(1);
			read_errors++;
			k_mem_slab_free(&i2s_slab, in);
			if (read_errors >= CONFIG_PENDANT_AUDIO_MAX_READ_ERRORS) {
				LOG_ERR("audio returned a short block %u times; stopping", read_errors);
				faulted = 1;
				run = 0;
				break;
			}
			continue;
		}

		void *out;

		if (k_mem_slab_alloc(&slab, &out, K_NO_WAIT) != 0) {
			stats.no_buffer++;
			store_note_drop(1);
			k_mem_slab_free(&i2s_slab, in);
			continue;
		}
		size_t n = audio_dsp_decim_i2s(&dec, in, I2S_BLOCK_FRAMES, out);

		k_mem_slab_free(&i2s_slab, in);
		if (n != BLK_SMP) {
			stats.read_errors++;
			store_note_drop(1);
			read_errors++;
			k_mem_slab_free(&slab, out);
			if (read_errors >= CONFIG_PENDANT_AUDIO_MAX_READ_ERRORS) {
				LOG_ERR("audio decimator produced a short block %u times; stopping", read_errors);
				faulted = 1;
				run = 0;
				break;
			}
			continue;
		}
		read_errors = 0;
		sup_beat(SUP_AUDIO);
		process_block(&ctx, out, n);
		k_mem_slab_free(&slab, out);
	}
	thread_exit(&ctx);
}

int audio_start(void)
{
	int err;

	if (run || active || thread_started) {
		return -EALREADY;
	}
	stats = (struct audio_stats){0};
	faulted = 0;
	k_sem_reset(&stopped);
	err = regulator_enable(mic_pwr);
	if (!err) {
		for (int i = 0; i < 10; i++) {
			err = i2s_configure(i2s, I2S_DIR_RX, &i2s_cfg);
			if (err != -EBUSY && err != -EINVAL) {
				break;
			}
			(void)i2s_reset(); /* previous stop/error left the driver in a non-READY state */
			k_msleep(1);
		}
		if (!err) {
			err = i2s_start_rx();
		}
		if (err && i2s_reset() == 0) {
			err = i2s_configure(i2s, I2S_DIR_RX, &i2s_cfg);
			if (!err) {
				err = i2s_start_rx();
			}
		}
	}
	if (err) {
		(void)regulator_disable(mic_pwr);
		return err;
	}
	active = 1;
	run = 1;
	k_thread_create(&thread, stack, K_THREAD_STACK_SIZEOF(stack), thread_fn, NULL, NULL, NULL, 5, 0,
			K_NO_WAIT);
	thread_started = 1;
	sup_arm(SUP_AUDIO, 2000);
	marker_set(M_REC, 1);
	return 0;
}

void audio_stop(void)
{
	if (!stop_thread()) {
		return;
	}
	if (active) {
		int err = i2s_reset();

		if (err) {
			LOG_WRN("I2S reset failed (%d)", err);
		}
		(void)regulator_disable(mic_pwr);
		active = 0;
	}
	sup_disarm(SUP_AUDIO);
	store_flush_drops();
	marker_set(M_REC, 0);
	atomic_set(&running, 0);
	faulted = 0;
}

#endif

#elif defined(CONFIG_PENDANT_AUDIO_SOURCE_NONE)

int audio_start(void)
{
	return -ENOTSUP;
}

void audio_stop(void)
{
}

#else
#error "No audio backend selected for this build"
#endif
