/* Append-only circular log of chunk records with power-fail-safe recovery.
 * Design and invariants: docs/spec/storage.md §5. Pure C; the device is a callback table so
 * the same code runs on Zephyr flash_area (Phase 0 RRAM), SPI NOR and SPI NAND, and on the
 * host flash simulator in test_ring.c. */
#ifndef RING_H
#define RING_H

#include <stddef.h>
#include <stdint.h>
#include "chunk.h"

struct ring_dev {
	void *ctx;
	int (*read)(void *ctx, uint32_t addr, void *buf, size_t n);
	/* Program only: bits go 1 -> 0. addr and n are multiples of wunit. */
	int (*write)(void *ctx, uint32_t addr, const void *buf, size_t n);
	int (*erase)(void *ctx, uint32_t addr); /* one block, addr is block aligned */
	uint32_t size;      /* bytes: 2 meta blocks, then data blocks */
	uint32_t block;     /* erase unit */
	uint32_t page;      /* program unit and record alignment, multiple of wunit and of 4, >= 32 */
	uint32_t wunit;     /* smallest program granularity (1 NOR, 16 RRAM, = page NAND) */
	uint8_t single_program; /* 1: a page can be programmed once (NAND); commit goes with the CRC */
	uint8_t *pbuf;      /* page bytes of scratch */
};

struct ring_rec {
	uint64_t pos;
	uint32_t seq, len;
};

struct ring {
	const struct ring_dev *d;
	uint32_t nblk, maxrec, probe;
	uint64_t tp, wp, blk_ready; /* absolute byte positions, never wrap; blk_ready = first block not known erased */
	uint32_t tail_seq, next_seq, acked, gen, boot;
	uint8_t slot;
};

/* Erases everything and writes the first metadata. maxrec = largest chunk_size() you will append. */
int ring_format(struct ring *r, const struct ring_dev *d, uint32_t maxrec);
/* Recovery after any power cut. Also increments the boot counter (one metadata commit). */
int ring_open(struct ring *r, const struct ring_dev *d, uint32_t maxrec);

/* Sets h->seq and h->boot; the caller fills type/codec/flags/mono_ms/rtc_ms/len. len % 4 == 0.
 * When full, drops the oldest records (acked or not) and records that in the metadata. */
int ring_append(struct ring *r, struct chunk_hdr *h, const uint8_t *payload);

/* Cumulative ACK from the phone. Ignored (returns -1) if it moves backwards or is beyond
 * the newest record. Frees nothing until ring_reclaim(). */
int ring_ack(struct ring *r, uint32_t upto);
/* Advances the tail past acked records and commits the metadata. */
int ring_reclaim(struct ring *r);
/* Erases up to n free blocks ahead of the write position, so appends need not erase. */
int ring_pre_erase(struct ring *r, uint32_t n);

/* Bytes the next appends can use without dropping anything: whole free blocks only (an erased
 * block is not required: erase-on-enter does that before the write). Conservative. */
uint64_t ring_free(const struct ring *r);

/* Bytes ahead of the write position already erased (RAM knowledge, lost at boot). When erases
 * cannot be afforded before the writes (low energy), this is the free space to count, not
 * ring_free(): blocks freed by an ACK are free but not erased until ring_pre_erase(). */
uint64_t ring_erased_ahead(const struct ring *r);

/* Overwritten-but-unacked range [first, last] to send as a gap record at sync start. */
int ring_gap(const struct ring *r, uint32_t *first, uint32_t *last);

/* Iterate retained records oldest first: prev = NULL for the first. Returns 0, or 1 at the end. */
int ring_find(const struct ring *r, const struct ring_rec *prev, struct ring_rec *out);
int ring_read(const struct ring *r, const struct ring_rec *rec, uint32_t off, void *buf, size_t n);

#endif
