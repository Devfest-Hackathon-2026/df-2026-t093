#include <string.h>
#include "chunk.h"

#define MAGIC  "LOSC"
#define COMMIT "CMT!"

static void put32(uint8_t *p, uint32_t v)
{
	for (int i = 0; i < 4; i++) {
		p[i] = v >> (8 * i);
	}
}

static void put64(uint8_t *p, uint64_t v)
{
	put32(p, (uint32_t)v);
	put32(p + 4, (uint32_t)(v >> 32));
}

static uint32_t get32(const uint8_t *p)
{
	return p[0] | p[1] << 8 | p[2] << 16 | (uint32_t)p[3] << 24;
}

static uint64_t get64(const uint8_t *p)
{
	return get32(p) | (uint64_t)get32(p + 4) << 32;
}

/* CRC-32/ISO-HDLC (zlib, check("123456789") = 0xCBF43926), 16-entry table. */
uint32_t chunk_crc32(uint32_t crc, const uint8_t *p, size_t n)
{
	static const uint32_t t[16] = {
		0x00000000, 0x1DB71064, 0x3B6E20C8, 0x26D930AC, 0x76DC4190, 0x6B6B51F4,
		0x4DB26158, 0x5005713C, 0xEDB88320, 0xF00F9344, 0xD6D6A3E8, 0xCB61B38C,
		0x9B64C2B0, 0x86D3D2D4, 0xA00AE278, 0xBDBDF21C,
	};

	crc = ~crc;
	while (n--) {
		crc ^= *p++;
		crc = (crc >> 4) ^ t[crc & 15];
		crc = (crc >> 4) ^ t[crc & 15];
	}
	return ~crc;
}

const uint8_t chunk_commit_word[4] = {'C', 'M', 'T', '!'};

int chunk_commit_ok(const uint8_t w[4])
{
	return !memcmp(w, COMMIT, 4);
}

int chunk_hdr_peek(const uint8_t b[CHUNK_HDR_SIZE], uint32_t *seq, uint32_t *len)
{
	if (memcmp(b, MAGIC, 4) || b[4] != CHUNK_VERSION) {
		return -1;
	}
	*seq = get32(b + 8);
	*len = get32(b + 28);
	return 0;
}

void chunk_hdr_encode(uint8_t out[CHUNK_HDR_SIZE], const struct chunk_hdr *h)
{
	memcpy(out, MAGIC, 4);
	out[4] = CHUNK_VERSION;
	out[5] = h->type;
	out[6] = h->codec;
	out[7] = h->flags;
	put32(out + 8, h->seq);
	put32(out + 12, h->boot);
	put32(out + 16, h->mono_ms);
	put64(out + 20, h->rtc_ms);
	put32(out + 28, h->len);
}

size_t chunk_encode(uint8_t *out, const struct chunk_hdr *h, const uint8_t *payload, int commit)
{
	uint8_t *t = out + CHUNK_HDR_SIZE + h->len;

	chunk_hdr_encode(out, h);
	memcpy(out + CHUNK_HDR_SIZE, payload, h->len);
	put32(t, chunk_crc32(0, out, CHUNK_HDR_SIZE + h->len));
	if (commit) {
		memcpy(t + 4, COMMIT, 4);
	} else {
		memset(t + 4, 0xff, 4);
	}
	return chunk_size(h->len);
}

enum chunk_st chunk_decode(const uint8_t *b, size_t avail, struct chunk_hdr *h,
			   const uint8_t **payload)
{
	static const uint8_t erased[4] = {0xff, 0xff, 0xff, 0xff};
	uint32_t len;
	const uint8_t *t;

	if (avail >= 4 && !memcmp(b, erased, 4)) {
		return CHUNK_EMPTY;
	}
	if (avail < CHUNK_HDR_SIZE || memcmp(b, MAGIC, 4) || b[4] != CHUNK_VERSION) {
		return CHUNK_BAD;
	}
	len = get32(b + 28);
	if (len > avail || chunk_size(len) > avail) {
		return CHUNK_BAD;
	}
	t = b + CHUNK_HDR_SIZE + len;
	if (!memcmp(t + 4, erased, 4)) {
		return CHUNK_INCOMPLETE;
	}
	if (memcmp(t + 4, COMMIT, 4) || get32(t) != chunk_crc32(0, b, CHUNK_HDR_SIZE + len)) {
		return CHUNK_BAD;
	}
	h->type = b[5];
	h->codec = b[6];
	h->flags = b[7];
	h->seq = get32(b + 8);
	h->boot = get32(b + 12);
	h->mono_ms = get32(b + 16);
	h->rtc_ms = get64(b + 20);
	h->len = len;
	*payload = b + CHUNK_HDR_SIZE;
	return CHUNK_OK;
}

void chunk_timefix_encode(uint8_t out[CHUNK_TIMEFIX_SIZE], uint64_t before, uint64_t after)
{
	put64(out, before);
	put64(out + 8, after);
}

void chunk_timefix_decode(const uint8_t *p, uint64_t *before, uint64_t *after)
{
	*before = get64(p);
	*after = get64(p + 8);
}

void chunk_event_encode(uint8_t out[CHUNK_EVENT_SIZE], uint8_t kind, uint32_t mono_ms)
{
	out[0] = kind;
	out[1] = out[2] = out[3] = 0;
	put32(out + 4, mono_ms);
}

void chunk_event_encode_arg(uint8_t out[CHUNK_EVENT_SIZE], uint8_t kind, uint8_t arg, uint32_t mono_ms)
{
	chunk_event_encode(out, kind, mono_ms);
	out[1] = arg;
}

void chunk_event_decode(const uint8_t *p, uint8_t *kind, uint32_t *mono_ms)
{
	*kind = p[0];
	*mono_ms = get32(p + 4);
}

void chunk_gap_encode(uint8_t out[CHUNK_GAP_SIZE], uint32_t first, uint32_t last, uint8_t reason)
{
	put32(out, first);
	put32(out + 4, last);
	out[8] = reason;
	out[9] = out[10] = out[11] = 0;
}

void chunk_gap_decode(const uint8_t *p, uint32_t *first, uint32_t *last, uint8_t *reason)
{
	*first = get32(p);
	*last = get32(p + 4);
	*reason = p[8];
}

enum chunk_rx_res chunk_rx_feed(struct chunk_rx *rx, const struct chunk_hdr *h,
				const uint8_t *payload)
{
	uint32_t first = h->seq, last = h->seq;
	uint8_t reason;

	if (h->type == CHUNK_GAP) {
		if (h->len != CHUNK_GAP_SIZE) {
			return CHUNK_RX_BAD;
		}
		chunk_gap_decode(payload, &first, &last, &reason);
		if (first == 0 || first != h->seq || last < first || last == UINT32_MAX) {
			return CHUNK_RX_BAD;
		}
	}
	if (h->seq == UINT32_MAX || last < rx->next) {
		return CHUNK_RX_DUP;
	}
	if (first > rx->next) {
		return CHUNK_RX_AHEAD;
	}
	rx->next = last + 1;
	return CHUNK_RX_ACCEPT;
}
