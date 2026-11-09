/* Record container for the NAND ring (Standard and Premium). Spec: docs/spec/formats.md.
 * Pure C, no Zephyr dependency. All multi-byte fields are little-endian on the wire. */
#ifndef CHUNK_H
#define CHUNK_H

#include <stddef.h>
#include <stdint.h>

#define CHUNK_HDR_SIZE     32
#define CHUNK_TRAILER_SIZE 8
#define CHUNK_VERSION      1
#define CHUNK_TIMEFIX_SIZE 16
#define CHUNK_GAP_SIZE     12
#define CHUNK_EVENT_SIZE   8

enum chunk_type { CHUNK_AUDIO = 1, CHUNK_TIMEFIX = 2, CHUNK_GAP = 3, CHUNK_EVENT = 4 };
enum chunk_codec { CHUNK_CODEC_NONE = 0, CHUNK_CODEC_IMA4_8K = 1 };
enum { CHUNK_F_RTC_INVALID = 1 };
enum { CHUNK_GAP_OVERWRITTEN = 1 };
enum { CHUNK_EV_BUTTON = 1, CHUNK_EV_AAD_WAKE = 2, CHUNK_EV_AAD_SLEEP = 3, CHUNK_EV_RESET = 4, CHUNK_EV_PRIVACY_ON = 5, CHUNK_EV_AUDIO_DROP = 6 };
/* CHUNK_EV_RESET carries the reset cause in the arg byte. */
enum { CHUNK_RESET_POR = 1, CHUNK_RESET_PIN = 2, CHUNK_RESET_WATCHDOG = 3, CHUNK_RESET_BROWNOUT = 4, CHUNK_RESET_SOFT = 5, CHUNK_RESET_OTHER = 6 };

struct chunk_hdr {
	uint8_t type, codec, flags;
	uint32_t seq;      /* one counter for all record types, starts at 1 */
	uint32_t boot;     /* boot counter */
	uint32_t mono_ms;  /* ms since boot at record start */
	uint64_t rtc_ms;   /* unix ms UTC at record start; 0 when CHUNK_F_RTC_INVALID */
	uint32_t len;      /* payload bytes */
};

enum chunk_st {
	CHUNK_OK,
	CHUNK_EMPTY,       /* erased: end of log */
	CHUNK_INCOMPLETE,  /* header looks valid, commit word never written */
	CHUNK_BAD,         /* anything else: bad magic/version/length/CRC, torn commit */
};

static inline size_t chunk_size(uint32_t len)
{
	uint64_t n = (uint64_t)CHUNK_HDR_SIZE + len + CHUNK_TRAILER_SIZE;

	return n > SIZE_MAX ? SIZE_MAX : (size_t)n;
}

uint32_t chunk_crc32(uint32_t crc, const uint8_t *p, size_t n);

/* Header bytes only (CHUNK_HDR_SIZE). chunk_encode() is this plus payload, CRC and commit. */
void chunk_hdr_encode(uint8_t out[CHUNK_HDR_SIZE], const struct chunk_hdr *h);

/* Cheap check of a header read on its own: 0 and seq/len filled if magic and version match.
 * Used by the ring to walk records without reading payloads. */
int chunk_hdr_peek(const uint8_t hdr[CHUNK_HDR_SIZE], uint32_t *seq, uint32_t *len);

/* The commit word as it appears on the wire, and a test for a 4-byte read of it. */
extern const uint8_t chunk_commit_word[4];
int chunk_commit_ok(const uint8_t w[4]);

/* Writes header, payload, CRC, then the commit word. commit=0 leaves it erased (0xFF),
 * which is what the device has after a power cut before the last program. */
size_t chunk_encode(uint8_t *out, const struct chunk_hdr *h, const uint8_t *payload, int commit);

/* buf must hold at least the bytes read from the device (>= CHUNK_HDR_SIZE preferred). */
enum chunk_st chunk_decode(const uint8_t *buf, size_t avail, struct chunk_hdr *h,
			   const uint8_t **payload);

void chunk_timefix_encode(uint8_t out[CHUNK_TIMEFIX_SIZE], uint64_t rtc_before_ms,
			  uint64_t rtc_after_ms);
void chunk_timefix_decode(const uint8_t *p, uint64_t *rtc_before_ms, uint64_t *rtc_after_ms);

/* Event record (type 4): something happened at mono_ms (this boot). Button presses are logged so
 * the ASR front end can treat the click in the recording as noise (docs/spec/audio.md §3.5). */
void chunk_event_encode(uint8_t out[CHUNK_EVENT_SIZE], uint8_t kind, uint32_t mono_ms);
void chunk_event_decode(const uint8_t *p, uint8_t *kind, uint32_t *mono_ms);
/* Same with the argument byte (reset cause, ...). */
void chunk_event_encode_arg(uint8_t out[CHUNK_EVENT_SIZE], uint8_t kind, uint8_t arg, uint32_t mono_ms);

/* Gap covers [first, last] sequence numbers; the record's own seq is first. */
void chunk_gap_encode(uint8_t out[CHUNK_GAP_SIZE], uint32_t first, uint32_t last, uint8_t reason);
void chunk_gap_decode(const uint8_t *p, uint32_t *first, uint32_t *last, uint8_t *reason);

/* Receiver (phone side): in-order cumulative ACK. Feed only records that passed
 * chunk_decode() == CHUNK_OK. */
struct chunk_rx {
	uint32_t next; /* next expected seq; start at 1 */
};
enum chunk_rx_res { CHUNK_RX_ACCEPT, CHUNK_RX_DUP, CHUNK_RX_AHEAD, CHUNK_RX_BAD };

enum chunk_rx_res chunk_rx_feed(struct chunk_rx *rx, const struct chunk_hdr *h,
				const uint8_t *payload);

static inline uint32_t chunk_rx_acked(const struct chunk_rx *rx)
{
	return rx->next - 1;
}

#endif
