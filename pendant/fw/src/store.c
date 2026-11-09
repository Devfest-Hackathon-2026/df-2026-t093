#include <errno.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/storage/flash_map.h>
#include "clock.h"
#include "marker.h"
#include "store.h"
#include "supervisor.h"

LOG_MODULE_REGISTER(store, LOG_LEVEL_INF);

#define PART PARTITION_ID(ring_partition)
#define MAXREC chunk_size(CONFIG_PENDANT_CHUNK_BYTES)

static const struct flash_area *fa;
static uint8_t pbuf[256];
static struct ring ring;
static struct ring_dev dev;
static K_MUTEX_DEFINE(lock);
static struct chunk_buf bufs[2];
static struct store_stats stats;
static int ready;
static struct k_spinlock drop_lock;
static uint32_t drop_count, drop_mono;
static uint64_t drop_rtc;
static uint8_t drop_flags;

static void drop_add(uint32_t n, uint32_t mono, uint64_t rtc, uint8_t flags)
{
	k_spinlock_key_t key = k_spin_lock(&drop_lock);

	if (!drop_count) {
		drop_mono = mono;
		drop_rtc = rtc;
		drop_flags = flags;
	}
	drop_count += n;
	k_spin_unlock(&drop_lock, key);
}

static uint32_t drop_take(uint32_t *mono, uint64_t *rtc, uint8_t *flags)
{
	k_spinlock_key_t key = k_spin_lock(&drop_lock);
	uint32_t n = drop_count;

	if (n) {
		*mono = drop_mono;
		*rtc = drop_rtc;
		*flags = drop_flags;
		drop_count = 0;
	}
	k_spin_unlock(&drop_lock, key);
	return n;
}

static int drop_events_locked(uint32_t *n, uint32_t mono, uint64_t rtc, uint8_t flags)
{
	while (*n) {
		uint8_t ev[CHUNK_EVENT_SIZE];
		struct chunk_hdr eh = {.type = CHUNK_EVENT, .codec = CHUNK_CODEC_NONE, .flags = flags,
				       .mono_ms = mono, .rtc_ms = rtc, .len = CHUNK_EVENT_SIZE};
		uint32_t k = MIN(*n, 255);

		chunk_event_encode_arg(ev, CHUNK_EV_AUDIO_DROP, k, mono);
		int err = ring_append(&ring, &eh, ev);

		if (err) {
			return err;
		}
		*n -= k;
	}
	return 0;
}

K_MSGQ_DEFINE(free_q, sizeof(struct chunk_buf *), 2, 4);
K_MSGQ_DEFINE(full_q, sizeof(struct chunk_buf *), 1, 4); /* 2 buffers: one held by audio, one queued */

static int fa_read(void *c, uint32_t a, void *b, size_t n)
{
	return flash_area_read(fa, a, b, n);
}

static int fa_write(void *c, uint32_t a, const void *b, size_t n)
{
	return flash_area_write(fa, a, b, n);
}

static int fa_erase(void *c, uint32_t a)
{
	return flash_area_flatten(fa, a, 4096);
}

static void writer(void *a, void *b, void *c)
{
	struct chunk_buf *cb;

	while (true) {
		struct chunk_hdr h;
		int64_t t0;
		int err;

		if (CONFIG_PENDANT_TEST_WRITER_STALL_MS) { /* before get: the queue, not the writer, holds the buffer */
			k_msleep(CONFIG_PENDANT_TEST_WRITER_STALL_MS);
		}
		if (k_msgq_get(&full_q, &cb, K_FOREVER)) {
			break;
		}
		if (stats.halted) {
			k_msgq_put(&free_q, &cb, K_FOREVER);
			continue;
		}
		t0 = k_uptime_get();
		marker_set(M_STORE, 1);
		k_mutex_lock(&lock, K_FOREVER);
		sup_arm(SUP_WRITER, CONFIG_PENDANT_WDT_TIMEOUT_MS - 1000);
		{
			uint32_t m;
			uint64_t r;
			uint8_t f;
			uint32_t n = drop_take(&m, &r, &f);

			if (n && drop_events_locked(&n, m, r, f)) {
				drop_add(n, m, r, f);
				stats.write_errors++;
				LOG_ERR("drop event failed");
			}
		}
		h = (struct chunk_hdr){.type = CHUNK_AUDIO, .codec = CHUNK_CODEC_IMA4_8K, .flags = cb->flags,
				       .mono_ms = cb->mono_ms, .rtc_ms = cb->rtc_ms, .len = cb->len};
		err = ring_append(&ring, &h, cb->payload);
		if (err) {
			uint32_t blocks = MAX(1, cb->len / 400);

			stats.write_errors++;
			LOG_ERR("ring_append: %d; recording a drop event", err);
			uint32_t left = blocks;

			if (drop_events_locked(&left, cb->mono_ms, cb->rtc_ms, cb->flags)) {
				drop_add(left, cb->mono_ms, cb->rtc_ms, cb->flags);
				stats.write_errors++;
				stats.halted = 1; /* do not let a later chunk reuse the failed sequence */
				LOG_ERR("drop event after failed append also failed; halting storage");
			}
		}
		sup_disarm(SUP_WRITER);
		k_mutex_unlock(&lock);
		marker_set(M_STORE, 0);
		stats.last_write_ms = k_uptime_get() - t0;
		stats.max_write_ms = MAX(stats.max_write_ms, stats.last_write_ms);
		if (!err) {
			stats.chunks_written++;
			stats.consecutive_errors = 0;
		} else {
			stats.consecutive_errors++;
			if (stats.consecutive_errors >= 3) {
				stats.halted = 1;
				LOG_ERR("storage failed 3 times; halting audio");
			}
		}
		k_msgq_put(&free_q, &cb, K_FOREVER);
	}
}

K_THREAD_DEFINE(writer_tid, 2048, writer, NULL, NULL, NULL, 7, 0, 0);

int store_init(void)
{
	int err = flash_area_open(PART, &fa);

	ready = 0;

	if (err) {
		return err;
	}
	dev = (struct ring_dev){.read = fa_read, .write = fa_write, .erase = fa_erase,
				.size = fa->fa_size, .block = 4096, .page = 256, .wunit = 16, .pbuf = pbuf};
	err = IS_ENABLED(CONFIG_PENDANT_FORMAT_ON_BOOT) ? -ENODATA : ring_open(&ring, &dev, MAXREC);
	if (err == -ENODATA) {
		LOG_INF("formatting ring");
		err = ring_format(&ring, &dev, MAXREC);
		err = err ? err : ring_open(&ring, &dev, MAXREC);
	}
	if (err) {
		return err;
	}
	LOG_INF("ring: boot %u, tail %u, next %u, acked %u, chunk %u B", ring.boot, ring.tail_seq,
		ring.next_seq, ring.acked, CONFIG_PENDANT_CHUNK_BYTES);
	for (int i = 0; i < 2; i++) {
		struct chunk_buf *b = &bufs[i];

		k_msgq_put(&free_q, &b, K_NO_WAIT);
	}
	ready = 1;
	return 0;
}

int store_ready(void)
{
	return ready;
}

struct chunk_buf *store_buf_get(k_timeout_t t)
{
	struct chunk_buf *b;

	return k_msgq_get(&free_q, &b, t) ? NULL : b;
}

void store_buf_release(struct chunk_buf *b)
{
	k_msgq_put(&free_q, &b, K_NO_WAIT);
}

void store_note_drop(uint32_t n)
{
	uint64_t rtc = clock_rtc_ms();

	drop_add(n, k_uptime_get_32(), rtc, rtc ? 0 : CHUNK_F_RTC_INVALID);
}

void store_flush_drops(void)
{
	uint32_t mono;
	uint64_t rtc;
	uint8_t flags;
	uint32_t n;

	if (!ready) {
		return;
	}
	k_mutex_lock(&lock, K_FOREVER);
	n = drop_take(&mono, &rtc, &flags);
	if (!n) {
		k_mutex_unlock(&lock);
		return;
	}
	sup_arm(SUP_WRITER, CONFIG_PENDANT_WDT_TIMEOUT_MS - 1000);
	if (drop_events_locked(&n, mono, rtc, flags)) {
		drop_add(n, mono, rtc, flags);
		LOG_ERR("drop flush failed");
	}
	sup_disarm(SUP_WRITER);
	k_mutex_unlock(&lock);
}

int store_buf_submit_timeout(struct chunk_buf *b, k_timeout_t t)
{
	return k_msgq_put(&full_q, &b, t);
}

struct ring *store_ring(void)
{
	return &ring;
}

void store_lock(void)
{
	k_mutex_lock(&lock, K_FOREVER);
}

void store_unlock(void)
{
	k_mutex_unlock(&lock);
}

int store_append(uint8_t type, const uint8_t *payload, uint32_t len)
{
	struct chunk_hdr h = {.type = type, .codec = CHUNK_CODEC_NONE, .mono_ms = k_uptime_get_32(),
			      .rtc_ms = clock_rtc_ms(), .len = len};
	int err;

	h.flags = h.rtc_ms ? 0 : CHUNK_F_RTC_INVALID;
	k_mutex_lock(&lock, K_FOREVER);
	err = ring_append(&ring, &h, payload);
	k_mutex_unlock(&lock);
	return err;
}

const struct store_stats *store_stats(void)
{
	return &stats;
}

int store_halted(void)
{
	return stats.halted;
}
