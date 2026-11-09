/* Pendant-side sync sender (issue #7), the logic that used to live in fw/src/ble.c. Pure C: the link, the
 * command queue and the ring lock are callbacks, so the same code runs on Zephyr and in the host tests with
 * loss injection (test_sync.c). Wire format: docs/spec/formats.md §6, §9-§11 and fw/src/ble.h. */
#ifndef SYNC_H
#define SYNC_H

#include <stddef.h>
#include <stdint.h>
#include "ring.h"

/* Same numbers as BLE_OP_*; SYNC_OP_RECLAIM is internal (queued on disconnect). */
enum { SYNC_OP_ACK = 1, SYNC_OP_SYNC = 2, SYNC_OP_TIME = 3, SYNC_OP_RESEND = 4, SYNC_OP_STOP = 5, SYNC_OP_RECLAIM = 0x80 };

struct sync_cmd {
	uint8_t op;
	uint32_t a, b;
	uint64_t t;
};

struct sync_stats {
	uint32_t bytes_sent, records_sent, resends, acks, syncs;
};

struct sync_io {
	void *ctx;
	/* One notification (seq u32, off u32, payload). Returns 0, or < 0 when the link is gone: the sync stops. */
	int (*tx)(void *ctx, const uint8_t *pkt, size_t n);
	/* Next queued control command: 1 and *c filled, or 0 if none. */
	int (*poll)(void *ctx, struct sync_cmd *c);
	/* Commands the sender does not own (TIME). May be NULL. */
	void (*other)(void *ctx, const struct sync_cmd *c);
	void (*lock)(void *ctx);   /* guards every access to the ring */
	void (*unlock)(void *ctx);
	uint32_t (*mono_ms)(void *ctx); /* stamps the gap records */
	uint64_t (*rtc_ms)(void *ctx);
	uint32_t frag;          /* payload bytes per notification */
	uint8_t *buf;           /* one record of scratch */
	size_t buf_size;
	struct sync_stats *st;
};

struct sync {
	struct ring *ring;
	const struct sync_io *io;
	uint32_t have;
	int restart, abort;
	struct { uint32_t seq, off; } rs[8]; /* RESENDs that arrived mid-sync: served after the current record */
	int nrs;
};

void sync_init(struct sync *s, struct ring *r, const struct sync_io *io);
/* Everything queued so far: ACK, RESEND, STOP, TIME, reclaim. A SYNC sets s->have and s->restart. */
void sync_drain(struct sync *s);
/* Sends [have + 1, next), a gap first if the ring lost some, then the end marker. Returns 0 when the end
 * marker went out, 1 if a STOP ended it, < 0 if the link failed. A new SYNC mid-transfer restarts it. */
int sync_run(struct sync *s, uint32_t have);
/* One RESEND outside a sync: record seq from byte off, or a gap record if it is gone. Returns 0 sent, 1 nothing
 * to send (record gone, no gap needed), < 0 link error. */
int sync_resend(struct sync *s, uint32_t seq, uint32_t off);

#endif
