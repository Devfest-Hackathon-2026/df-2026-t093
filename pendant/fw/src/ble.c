#include <errno.h>
#include <string.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include "audio.h"
#include "ble.h"
#include "clock.h"
#include "marker.h"
#include "store.h"
#include "supervisor.h"
#include "sync.h"

LOG_MODULE_REGISTER(ble, LOG_LEVEL_INF);

#define UUID_BASE(x) BT_UUID_128_ENCODE(0xa3c1f000 + (x), 0x2b6b, 0x4d0c, 0x9f6e, 0x6c6966654f53)
static struct bt_uuid_128 svc_uuid = BT_UUID_INIT_128(UUID_BASE(0));
static struct bt_uuid_128 data_uuid = BT_UUID_INIT_128(UUID_BASE(1));
static struct bt_uuid_128 ctrl_uuid = BT_UUID_INIT_128(UUID_BASE(2));
static struct bt_uuid_128 info_uuid = BT_UUID_INIT_128(UUID_BASE(3));

#ifdef CONFIG_PENDANT_BLE_SECURE
#define PERM_R BT_GATT_PERM_READ_ENCRYPT
#define PERM_W BT_GATT_PERM_WRITE_ENCRYPT
#else
#define PERM_R BT_GATT_PERM_READ
#define PERM_W BT_GATT_PERM_WRITE
#endif

K_MSGQ_DEFINE(cmdq, sizeof(struct sync_cmd), 16, 4);

static struct bt_conn *conn;
static volatile int notify_on;
static struct ble_stats stats;
static struct sync_stats sst;
static int adv_on;

static uint32_t rd32(const uint8_t *p)
{
	return p[0] | p[1] << 8 | p[2] << 16 | (uint32_t)p[3] << 24;
}

static void wr32(uint8_t *p, uint32_t v)
{
	for (int i = 0; i < 4; i++) {
		p[i] = v >> (8 * i);
	}
}

static void ccc_changed(const struct bt_gatt_attr *attr, uint16_t value)
{
	notify_on = value == BT_GATT_CCC_NOTIFY;
}

static ssize_t ctrl_write(struct bt_conn *c, const struct bt_gatt_attr *attr, const void *buf, uint16_t len,
			  uint16_t off, uint8_t flags)
{
	const uint8_t *p = buf;
	struct sync_cmd cm = {.op = len ? p[0] : 0};

	if (len < 1 || off) {
		return BT_GATT_ERR(BT_ATT_ERR_INVALID_ATTRIBUTE_LEN);
	}
	switch (cm.op) {
	case BLE_OP_ACK:
	case BLE_OP_SYNC:
		if (len != 5) {
			return BT_GATT_ERR(BT_ATT_ERR_INVALID_ATTRIBUTE_LEN);
		}
		cm.a = rd32(p + 1);
		break;
	case BLE_OP_TIME:
		if (len != 9) {
			return BT_GATT_ERR(BT_ATT_ERR_INVALID_ATTRIBUTE_LEN);
		}
		cm.t = rd32(p + 1) | (uint64_t)rd32(p + 5) << 32;
		if (!clock_time_valid(cm.t)) {
			return BT_GATT_ERR(BT_ATT_ERR_VALUE_NOT_ALLOWED);
		}
		break;
	case BLE_OP_RESEND:
		if (len != 9) {
			return BT_GATT_ERR(BT_ATT_ERR_INVALID_ATTRIBUTE_LEN);
		}
		cm.a = rd32(p + 1);
		cm.b = rd32(p + 5);
		break;
	case BLE_OP_STOP:
		if (len != 1) {
			return BT_GATT_ERR(BT_ATT_ERR_INVALID_ATTRIBUTE_LEN);
		}
		break;
	default:
		return BT_GATT_ERR(BT_ATT_ERR_VALUE_NOT_ALLOWED);
	}
	if (k_msgq_put(&cmdq, &cm, K_NO_WAIT)) {
		stats.cmd_dropped++; /* queue full: the phone sees an error only for a write with response */
		return BT_GATT_ERR(BT_ATT_ERR_INSUFFICIENT_RESOURCES);
	}
	return len;
}

static ssize_t info_read(struct bt_conn *c, const struct bt_gatt_attr *attr, void *buf, uint16_t len, uint16_t off)
{
	uint8_t d[32] = {1, audio_running()};
	struct ring *r = store_ring();
	uint64_t t = clock_rtc_ms();

	if (!store_ready() || store_try_lock()) {
		return BT_GATT_ERR(BT_ATT_ERR_INSUFFICIENT_RESOURCES);
	}
	{
		wr32(d + 4, r->boot);
		wr32(d + 8, r->tail_seq);
		wr32(d + 12, r->next_seq);
		wr32(d + 16, r->acked);
		store_unlock();
	}
	wr32(d + 20, k_uptime_get_32() / 1000);
	wr32(d + 24, (uint32_t)t);
	wr32(d + 28, (uint32_t)(t >> 32));
	return bt_gatt_attr_read(c, attr, buf, len, off, d, sizeof(d));
}

BT_GATT_SERVICE_DEFINE(pendant_svc, BT_GATT_PRIMARY_SERVICE(&svc_uuid),
	BT_GATT_CHARACTERISTIC(&data_uuid.uuid, BT_GATT_CHRC_NOTIFY, BT_GATT_PERM_NONE, NULL, NULL, NULL),
	BT_GATT_CCC(ccc_changed, PERM_R | PERM_W),
	BT_GATT_CHARACTERISTIC(&ctrl_uuid.uuid, BT_GATT_CHRC_WRITE | BT_GATT_CHRC_WRITE_WITHOUT_RESP, PERM_W, NULL,
			       ctrl_write, NULL),
	BT_GATT_CHARACTERISTIC(&info_uuid.uuid, BT_GATT_CHRC_READ, PERM_R, info_read, NULL, NULL));

#define DATA_ATTR (&pendant_svc.attrs[2])

/* ---- connection ---- */

static K_MUTEX_DEFINE(conn_lock);

static struct bt_conn *conn_get(void)
{
	struct bt_conn *c;

	k_mutex_lock(&conn_lock, K_FOREVER);
	c = conn ? bt_conn_ref(conn) : NULL;
	k_mutex_unlock(&conn_lock);
	return c;
}

static void connected(struct bt_conn *c, uint8_t err)
{
	if (err) {
		adv_on = 0;
		marker_set(M_BLE, 0);
		return;
	}
	k_mutex_lock(&conn_lock, K_FOREVER);
	conn = bt_conn_ref(c);
	k_mutex_unlock(&conn_lock);
	adv_on = 0;
	stats.connects++;
	LOG_INF("connected");
	bt_conn_le_phy_update(c, BT_CONN_LE_PHY_PARAM_2M);
	bt_conn_le_data_len_update(c, BT_LE_DATA_LEN_PARAM_MAX);
	bt_conn_le_param_update(c, BT_LE_CONN_PARAM(6, 12, 0, 400)); /* ask for 7.5-15 ms */
}

static void disconnected(struct bt_conn *c, uint8_t reason)
{
	struct sync_cmd cm = {.op = BLE_OP_STOP};

	LOG_INF("disconnected 0x%02x", reason);
	notify_on = 0;
	k_msgq_put(&cmdq, &cm, K_NO_WAIT);
	cm.op = SYNC_OP_RECLAIM;
	k_msgq_put(&cmdq, &cm, K_NO_WAIT);
	k_mutex_lock(&conn_lock, K_FOREVER);
	if (conn == c) {
		bt_conn_unref(conn);
		conn = NULL;
	}
	k_mutex_unlock(&conn_lock);
	marker_set(M_BLE, 0);
}

static void le_param_updated(struct bt_conn *c, uint16_t interval, uint16_t latency, uint16_t timeout)
{
	stats.interval_125us = interval * 10; /* units of 1.25 ms -> 0.125 ms */
	LOG_INF("conn params: interval %u.%02u ms, latency %u, timeout %u ms", interval * 5 / 4, interval * 5 % 4 * 25, latency,
		timeout * 10);
}

static void le_data_len_updated(struct bt_conn *c, struct bt_conn_le_data_len_info *i)
{
	LOG_INF("data length: tx %u rx %u", i->tx_max_len, i->rx_max_len);
}

static void le_phy_updated(struct bt_conn *c, struct bt_conn_le_phy_info *p)
{
	stats.phy_2m = p->tx_phy == BT_GAP_LE_PHY_2M;
	LOG_INF("phy: tx %u rx %u", p->tx_phy, p->rx_phy);
}

BT_CONN_CB_DEFINE(conn_cb) = {.connected = connected, .disconnected = disconnected, .le_phy_updated = le_phy_updated,
	.le_param_updated = le_param_updated, .le_data_len_updated = le_data_len_updated};

static const struct bt_data ad[] = {
	BT_DATA_BYTES(BT_DATA_FLAGS, BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR),
	BT_DATA_BYTES(BT_DATA_UUID128_ALL, UUID_BASE(0)),
};
static const struct bt_data sd[] = {
	BT_DATA(BT_DATA_NAME_COMPLETE, CONFIG_BT_DEVICE_NAME, sizeof(CONFIG_BT_DEVICE_NAME) - 1),
};

void ble_adv_start(void)
{
	int err = 0;

	for (int i = 0; i < 3; i++) {
		err = bt_le_adv_start(BT_LE_ADV_CONN_FAST_1, ad, ARRAY_SIZE(ad), sd, ARRAY_SIZE(sd));
		if (!err || err == -EALREADY) {
			err = 0;
			break;
		}
		k_msleep(100);
	}
	adv_on = !err;
	marker_set(M_BLE, !err);
	LOG_INF("advertising: %d", err);
}

void ble_adv_stop(void)
{
	bt_le_adv_stop();
	adv_on = 0;
	marker_set(M_BLE, conn != NULL);
}

const struct ble_stats *ble_stats(void)
{
	{
		struct bt_conn *c = conn_get();

		stats.mtu = c ? bt_gatt_get_mtu(c) : 0;
		if (c) {
			bt_conn_unref(c);
		}
	}
	stats.bytes_sent = sst.bytes_sent;
	stats.records_sent = sst.records_sent;
	stats.resends = sst.resends;
	stats.acks = sst.acks;
	stats.syncs = sst.syncs;
	return &stats;
}

/* ---- sync thread ---- */

static int send(const uint8_t *pkt, size_t n)
{
	struct bt_conn *c = conn_get();
	int err = -ENOTCONN;

	if (!c) {
		return err;
	}
	for (int tries = 0; tries < 2000; tries++) {
		sup_beat(SUP_BLE);
		if (!notify_on) {
			err = -ENOTCONN;
			break;
		}
		err = bt_gatt_notify(c, DATA_ATTR, pkt, n);
		if (err != -ENOMEM) {
			break;
		}
		stats.notify_nomem++;
		k_sleep(K_MSEC(1));
	}
	bt_conn_unref(c);
	return err;
}

static uint8_t recbuf[CHUNK_HDR_SIZE + CONFIG_PENDANT_CHUNK_BYTES + CHUNK_TRAILER_SIZE];

static int io_tx(void *c, const uint8_t *pkt, size_t n)
{
	return send(pkt, n);
}

static int io_poll(void *c, struct sync_cmd *cm)
{
	return !k_msgq_get(&cmdq, cm, K_NO_WAIT);
}

static void io_other(void *c, const struct sync_cmd *cm) /* TIME */
{
	uint8_t p[CHUNK_TIMEFIX_SIZE];
	uint64_t before;
	int err;

	if (!store_ready()) {
		LOG_WRN("TIME rejected while storage is unavailable");
		return;
	}
	before = clock_rtc_ms();
	err = clock_set(cm->t);
	if (err) {
		LOG_WRN("TIME rejected (%d)", err);
		return;
	}
	chunk_timefix_encode(p, before, cm->t);
	LOG_INF("time set, timefix append: %d", store_append(CHUNK_TIMEFIX, p, sizeof(p)));
}

static void io_lock(void *c)
{
	store_lock();
}

static void io_unlock(void *c)
{
	store_unlock();
}

static uint32_t io_mono(void *c)
{
	return k_uptime_get_32();
}

static uint64_t io_rtc(void *c)
{
	return clock_rtc_ms();
}

static struct sync_io io = {.tx = io_tx, .poll = io_poll, .other = io_other, .lock = io_lock, .unlock = io_unlock,
			    .mono_ms = io_mono, .rtc_ms = io_rtc, .buf = recbuf, .buf_size = sizeof(recbuf), .st = &sst};

static void do_sync(struct sync *s, uint32_t have)
{
	int64_t t0 = k_uptime_get();

	if (!store_ready()) {
		return;
	}
	uint32_t b0 = sst.bytes_sent;
	int r;

	struct bt_conn *c = conn_get();

	if (!c) {
		return;
	}
	io.frag = MIN(bt_gatt_get_mtu(c) - 3 - 8, 244);
	bt_conn_unref(c);
	LOG_INF("sync from %u", have);
	r = sync_run(s, have);
	stats.last_sync_ms = k_uptime_get() - t0;
	stats.last_sync_bytes = sst.bytes_sent - b0;
	LOG_INF("sync %s: %u B in %u ms (%u kB/s), 2M %d, nomem retries %u, resends %u", r ? "ended early" : "done",
		stats.last_sync_bytes, stats.last_sync_ms, stats.last_sync_ms ? stats.last_sync_bytes / stats.last_sync_ms : 0,
		stats.phy_2m, stats.notify_nomem, sst.resends);
}

static void sync_thread(void *a, void *b, void *c)
{
	struct sync_cmd cm;

	for (;;) {
		struct sync s;

		if (!store_ready()) {
			k_msgq_purge(&cmdq);
			sup_beat(SUP_BLE);
			k_sleep(K_SECONDS(1));
			continue;
		}
		sync_init(&s, store_ring(), &io);
		sup_beat(SUP_BLE);
		if (k_msgq_get(&cmdq, &cm, K_SECONDS(5))) {
			if (!conn && IS_ENABLED(CONFIG_PENDANT_BLE)) {
				ble_adv_start();
			}
			continue; /* idle: wake up now and then so the supervisor hears us */
		}
		switch (cm.op) {
		case BLE_OP_SYNC:
			do_sync(&s, cm.a);
			break;
		case BLE_OP_RESEND: {
			struct bt_conn *c = conn_get();

			io.frag = c ? MIN(bt_gatt_get_mtu(c) - 3 - 8, 244) : 20;
			if (c) {
				bt_conn_unref(c);
			}
			LOG_INF("resend %u from %u: %d", cm.a, cm.b, sync_resend(&s, cm.a, cm.b));
			break;
		}
		default: /* ACK, TIME, STOP, reclaim: same handling as between records */
			k_msgq_put(&cmdq, &cm, K_NO_WAIT);
			sync_drain(&s);
			if (s.restart) { /* a SYNC was queued behind them (TIME, then SYNC) */
				do_sync(&s, s.have);
			}
		}
		if (!conn && !adv_on && IS_ENABLED(CONFIG_PENDANT_BLE)) {
			ble_adv_start(); /* bench build: stay discoverable */
		}
	}
}

K_THREAD_DEFINE(sync_tid, 3072, sync_thread, NULL, NULL, NULL, 6, 0, 0);

int ble_enable(void)
{
	int err = bt_enable(NULL);

	if (!err) {
		sup_arm(SUP_BLE, 40000); /* an ATT notification may wait 30 s for a dead link */
	}
	return err;
}

int ble_init(void)
{
	int err = 0;

	for (int i = 0; i < 3; i++) {
		err = ble_enable();
		if (!err) {
			break;
		}
		(void)bt_disable();
		k_msleep(100);
	}
	if (!err) {
		ble_adv_start();
	}
	return err;
}
