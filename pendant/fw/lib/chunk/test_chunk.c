/* Host test: cc -std=c99 -Wall -Wextra -Werror -o /tmp/t test_chunk.c chunk.c && /tmp/t */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "chunk.h"

static struct chunk_hdr mk(uint8_t type, uint32_t seq, uint32_t len)
{
	struct chunk_hdr h = {.type = type, .codec = CHUNK_CODEC_IMA4_8K, .seq = seq, .boot = 7,
			      .mono_ms = 123456, .rtc_ms = 1760000000123ULL, .len = len};
	return h;
}

static enum chunk_st dec(const uint8_t *b, size_t n, struct chunk_hdr *h)
{
	const uint8_t *p;

	return chunk_decode(b, n, h, &p);
}

/* Feeds a decoded record to the receiver, like the phone does after CRC + commit pass. */
static enum chunk_rx_res feed(struct chunk_rx *rx, const uint8_t *rec, size_t n)
{
	struct chunk_hdr h;
	const uint8_t *p;

	assert(chunk_decode(rec, n, &h, &p) == CHUNK_OK);
	return chunk_rx_feed(rx, &h, p);
}

int main(void)
{
	uint8_t pay[100], buf[256], rec[4][256];
	struct chunk_hdr h, o;
	const uint8_t *p;
	size_t n;

	for (int i = 0; i < 100; i++) {
		pay[i] = i * 7;
	}

	/* CRC-32 check value and chaining. */
	assert(chunk_crc32(0, (const uint8_t *)"123456789", 9) == 0xCBF43926);
	assert(chunk_crc32(chunk_crc32(0, (const uint8_t *)"12345", 5),
			   (const uint8_t *)"6789", 4) == 0xCBF43926);

	/* Round trip, and the wire bytes are fixed (little-endian, 32 + len + 8). */
	h = mk(CHUNK_AUDIO, 42, 100);
	n = chunk_encode(buf, &h, pay, 1);
	assert(n == 140 && n == chunk_size(100));
	assert(!memcmp(buf, "LOSC", 4) && buf[4] == 1 && buf[5] == 1 && buf[8] == 42);
	assert(!memcmp(buf + 136, "CMT!", 4));
	assert(chunk_decode(buf, n, &o, &p) == CHUNK_OK);
	assert(o.type == h.type && o.codec == h.codec && o.flags == h.flags && o.seq == 42 &&
	       o.boot == 7 && o.mono_ms == 123456 && o.rtc_ms == h.rtc_ms && o.len == 100);
	assert(!memcmp(p, pay, 100));
	/* Trailing erased padding up to the page end does not matter. */
	memset(buf + n, 0xff, sizeof(buf) - n);
	assert(chunk_decode(buf, sizeof(buf), &o, &p) == CHUNK_OK);

	/* Every single-bit flip in header, payload and CRC is detected. */
	for (size_t i = 0; i < n - 4; i++) {
		for (int b = 0; b < 8; b++) {
			buf[i] ^= 1 << b;
			assert(dec(buf, n, &o) != CHUNK_OK);
			buf[i] ^= 1 << b;
		}
	}
	/* Torn commit word is bad, not incomplete. */
	buf[n - 1] ^= 0x01;
	assert(dec(buf, n, &o) == CHUNK_BAD);
	buf[n - 1] ^= 0x01;
	assert(dec(buf, n, &o) == CHUNK_OK);

	/* Power cut at every byte: record prefix, rest erased. Never valid before the last byte. */
	n = chunk_encode(buf, &h, pay, 1);
	for (size_t k = 0; k < n; k++) {
		uint8_t torn[256];

		memcpy(torn, buf, k);
		memset(torn + k, 0xff, sizeof(torn) - k);
		assert(dec(torn, sizeof(torn), &o) != CHUNK_OK);
	}
	/* Commit never written: incomplete. Nothing written: end of log. */
	chunk_encode(buf, &h, pay, 0);
	assert(dec(buf, n, &o) == CHUNK_INCOMPLETE);
	memset(buf, 0xff, sizeof(buf));
	assert(dec(buf, sizeof(buf), &o) == CHUNK_EMPTY);
	/* Truncated read and absurd length. */
	n = chunk_encode(buf, &h, pay, 1);
	assert(dec(buf, n - 1, &o) == CHUNK_BAD);
	buf[28] = buf[29] = buf[30] = buf[31] = 0xfe;
	assert(dec(buf, n, &o) == CHUNK_BAD);

	/* Time correction record. */
	{
		uint8_t tp[CHUNK_TIMEFIX_SIZE], b[128];
		uint64_t before, after;

		chunk_timefix_encode(tp, 1760000000000ULL, 1760000001500ULL);
		h = mk(CHUNK_TIMEFIX, 5, sizeof(tp));
		h.codec = CHUNK_CODEC_NONE;
		h.flags = CHUNK_F_RTC_INVALID;
		n = chunk_encode(b, &h, tp, 1);
		assert(chunk_decode(b, n, &o, &p) == CHUNK_OK && o.type == CHUNK_TIMEFIX);
		assert(o.flags & CHUNK_F_RTC_INVALID);
		chunk_timefix_decode(p, &before, &after);
		assert(before == 1760000000000ULL && after == 1760000001500ULL);
	}

	/* Event record: a button press at a monotonic time. */
	{
		uint8_t ep[CHUNK_EVENT_SIZE], eb[128], kind;
		uint32_t at;

		{
			struct chunk_hdr gh = {.type = CHUNK_GAP, .seq = 5, .len = 0};
			struct chunk_rx rx = {.next = 5};
			uint8_t gr[CHUNK_HDR_SIZE + CHUNK_TRAILER_SIZE];

			uint8_t dummy = 0;

			chunk_encode(gr, &gh, &dummy, 1);
			assert(chunk_decode(gr, sizeof(gr), &o, &p) == CHUNK_OK);
			assert(chunk_rx_feed(&rx, &o, p) == CHUNK_RX_BAD);
		}
		chunk_event_encode_arg(ep, CHUNK_EV_AUDIO_DROP, 7, 654321);
		chunk_event_decode(ep, &kind, &at);
		assert(kind == CHUNK_EV_AUDIO_DROP && ep[1] == 7 && at == 654321);
		chunk_event_encode(ep, CHUNK_EV_BUTTON, 123456);
		h = mk(CHUNK_EVENT, 9, sizeof(ep));
		h.codec = CHUNK_CODEC_NONE;
		n = chunk_encode(eb, &h, ep, 1);
		assert(chunk_decode(eb, n, &o, &p) == CHUNK_OK && o.type == CHUNK_EVENT);
		chunk_event_decode(p, &kind, &at);
		assert(kind == CHUNK_EV_BUTTON && at == 123456);
		chunk_event_encode_arg(ep, CHUNK_EV_RESET, CHUNK_RESET_WATCHDOG, 7);
		assert(ep[0] == CHUNK_EV_RESET && ep[1] == CHUNK_RESET_WATCHDOG && !ep[2] && !ep[3]);
		chunk_event_decode(ep, &kind, &at);
		assert(kind == CHUNK_EV_RESET && at == 7);
	}

	/* Receiver: in-order cumulative ACK, reorder, duplicates, gap. */
	for (uint32_t s = 1; s <= 4; s++) {
		h = mk(CHUNK_AUDIO, s, 10);
		assert(chunk_encode(rec[s - 1], &h, pay, 1) == chunk_size(10));
	}
	size_t rn = chunk_size(10);
	struct chunk_rx rx = {.next = 1};

	assert(chunk_rx_acked(&rx) == 0);
	assert(feed(&rx, rec[0], rn) == CHUNK_RX_ACCEPT && chunk_rx_acked(&rx) == 1);
	assert(feed(&rx, rec[0], rn) == CHUNK_RX_DUP && chunk_rx_acked(&rx) == 1);
	assert(feed(&rx, rec[2], rn) == CHUNK_RX_AHEAD && chunk_rx_acked(&rx) == 1); /* 3 before 2 */
	assert(feed(&rx, rec[1], rn) == CHUNK_RX_ACCEPT && chunk_rx_acked(&rx) == 2);
	assert(feed(&rx, rec[2], rn) == CHUNK_RX_ACCEPT && chunk_rx_acked(&rx) == 3);
	assert(feed(&rx, rec[1], rn) == CHUNK_RX_DUP && chunk_rx_acked(&rx) == 3); /* stale re-send */

	/* Overwritten records 4..9: the gap record skips the receiver over them. */
	{
		uint8_t gp[CHUNK_GAP_SIZE], g[64], g2[64], a10[64];
		uint32_t first, last;
		uint8_t reason;

		chunk_gap_encode(gp, 4, 9, CHUNK_GAP_OVERWRITTEN);
		h = mk(CHUNK_GAP, 4, sizeof(gp));
		h.codec = CHUNK_CODEC_NONE;
		n = chunk_encode(g, &h, gp, 1);
		assert(chunk_decode(g, n, &o, &p) == CHUNK_OK && o.type == CHUNK_GAP);
		chunk_gap_decode(p, &first, &last, &reason);
		assert(first == 4 && last == 9 && reason == CHUNK_GAP_OVERWRITTEN);
		h = mk(CHUNK_AUDIO, 10, 10);
		chunk_encode(a10, &h, pay, 1);

		assert(feed(&rx, a10, rn) == CHUNK_RX_AHEAD && chunk_rx_acked(&rx) == 3);
		assert(feed(&rx, g, n) == CHUNK_RX_ACCEPT && chunk_rx_acked(&rx) == 9);
		assert(feed(&rx, g, n) == CHUNK_RX_DUP && chunk_rx_acked(&rx) == 9);
		assert(feed(&rx, a10, rn) == CHUNK_RX_ACCEPT && chunk_rx_acked(&rx) == 10);
		/* A gap that overlaps what was already received only advances to its end. */
		chunk_gap_encode(gp, 8, 12, CHUNK_GAP_OVERWRITTEN);
		h = mk(CHUNK_GAP, 8, sizeof(gp));
		n = chunk_encode(g2, &h, gp, 1);
		assert(feed(&rx, g2, n) == CHUNK_RX_ACCEPT && chunk_rx_acked(&rx) == 12);
	}

	puts("chunk: all tests passed");
	return 0;
}
