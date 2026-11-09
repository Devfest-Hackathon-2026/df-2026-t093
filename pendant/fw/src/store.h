/* The ring on the internal RRAM partition (slot1), plus a writer thread so the audio path
 * never waits for a flash program. */
#ifndef STORE_H
#define STORE_H

#include "ring.h"

struct chunk_buf {
	uint8_t payload[CONFIG_PENDANT_CHUNK_BYTES];
	uint32_t len, mono_ms;
	uint64_t rtc_ms;
	uint8_t flags;
};

int store_init(void);
int store_ready(void); /* 1 after a successful ring_open/format */
/* Audio side: take an empty buffer (blocks), fill it, hand it back full. */
struct chunk_buf *store_buf_get(k_timeout_t t);
/* Returns -EAGAIN on timeout instead of blocking forever. The audio thread polls this so stop can wake it. */
int store_buf_submit_timeout(struct chunk_buf *b, k_timeout_t t);
void store_buf_release(struct chunk_buf *b); /* give an unused buffer back */
/* The audio path dropped N 100 ms blocks; the writer records an event before the next chunk. */
void store_note_drop(uint32_t n);
/* Flush pending drops after the audio thread has stopped. */
void store_flush_drops(void);

/* Sync side. The ring is not thread safe: hold the lock for any call on it. */
struct ring *store_ring(void);
void store_lock(void);
int store_try_lock(void); /* -EBUSY if the writer holds it; no blocking */
void store_unlock(void);
/* Appends a small record (timefix) under the lock. */
int store_append(uint8_t type, const uint8_t *payload, uint32_t len);

struct store_stats {
	uint32_t chunks_written, write_errors, max_write_ms, last_write_ms;
	uint32_t consecutive_errors;
	uint8_t halted;
};
const struct store_stats *store_stats(void);
int store_halted(void);

#endif
