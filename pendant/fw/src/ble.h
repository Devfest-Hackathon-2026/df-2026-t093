/* BLE sync service. Wire format: docs/spec/formats.md §6, §9 (fragments, cumulative ACK) plus the control ops below. */
#ifndef BLE_H
#define BLE_H

#include <stdint.h>

/* CTRL characteristic (phone -> pendant), first byte is the op, little-endian fields:
 *   0x01 ACK    upto_seq u32     cumulative; every record up to upto_seq verified (CRC + commit)
 *   0x02 SYNC   have u32         start sending after `have`; gap first if the ring lost some
 *   0x03 TIME   unix_ms u64      set the clock; the pendant appends a timefix record
 *   0x04 RESEND seq u32, off u32 send record seq again from byte off
 *   0x05 STOP                    abort the transfer
 * DATA characteristic (pendant -> phone, notifications): seq u32, off u32, bytes.
 *   seq 0 is the end marker: off = next sequence number (everything older was sent).
 */
enum { BLE_OP_ACK = 1, BLE_OP_SYNC = 2, BLE_OP_TIME = 3, BLE_OP_RESEND = 4, BLE_OP_STOP = 5 };

int ble_enable(void); /* controller up, not advertising */
int ble_init(void);   /* ble_enable() + start advertising */
void ble_adv_start(void);
void ble_adv_stop(void);

struct ble_stats {
	uint32_t cmd_dropped, resends, connects, syncs, bytes_sent, records_sent, acks, notify_nomem, last_sync_ms, last_sync_bytes;
	uint16_t mtu, interval_125us;
	uint8_t phy_2m;
};
const struct ble_stats *ble_stats(void);

#endif
