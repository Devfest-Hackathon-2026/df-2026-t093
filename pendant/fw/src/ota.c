#include <zephyr/dfu/mcuboot.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/mgmt/mcumgr/mgmt/callbacks.h>
#include <zephyr/mgmt/mcumgr/mgmt/mgmt.h>
#include <zephyr/mgmt/mcumgr/grp/img_mgmt/img_mgmt_callbacks.h>
#include <zephyr/mgmt/mcumgr/grp/os_mgmt/os_mgmt_callbacks.h>
#include <zephyr/storage/flash_map.h>
#include "ota.h"

LOG_MODULE_REGISTER(ota, LOG_LEVEL_INF);

/* Whether the supply is good enough to take an image. Standard decides this from the battery state
 * machine (docs/spec/power.md §9.1: NORMAL only); Phase 0 has no battery sensing, so the policy is a stub that a
 * test flag can turn into a refusal. */
static int ota_allowed(void)
{
	return !IS_ENABLED(CONFIG_PENDANT_TEST_OTA_DENY);
}

static enum mgmt_cb_return upload_gate(uint32_t event, enum mgmt_cb_return prev, int32_t *rc, uint16_t *group,
				       bool *abort_more, void *data, size_t data_size)
{
	if (!ota_allowed()) {
		LOG_WRN("OTA operation 0x%x refused: supply not good enough", event);
		*rc = MGMT_ERR_EACCESSDENIED;
		*abort_more = true;
		return MGMT_CB_ERROR_RC;
	}
	return MGMT_CB_OK;
}

static struct mgmt_callback gate_cbs[] = {
	{.callback = upload_gate, .event_id = MGMT_EVT_OP_IMG_MGMT_DFU_CHUNK},
	{.callback = upload_gate, .event_id = MGMT_EVT_OP_IMG_MGMT_DFU_STARTED},
	{.callback = upload_gate, .event_id = MGMT_EVT_OP_IMG_MGMT_IMAGE_SLOT_STATE},
	{.callback = upload_gate, .event_id = MGMT_EVT_OP_OS_MGMT_RESET},
};
static int64_t booted_at;

void ota_init(void)
{
	struct mcuboot_img_header hdr;

	for (size_t i = 0; i < ARRAY_SIZE(gate_cbs); i++) {
		mgmt_callback_register(&gate_cbs[i]);
	}
	booted_at = k_uptime_get();
	if (!boot_read_bank_header(PARTITION_ID(slot0_partition), &hdr, sizeof(hdr))) {
		LOG_INF("running image %u.%u.%u+%u, confirmed %d", hdr.h.v1.sem_ver.major, hdr.h.v1.sem_ver.minor,
			hdr.h.v1.sem_ver.revision, hdr.h.v1.sem_ver.build_num, boot_is_img_confirmed());
	}
}

void ota_poll(void)
{
	/* A test image that survived the confirm period (the supervisor would have reset us otherwise). */
	if (!boot_is_img_confirmed() && k_uptime_get() - booted_at > CONFIG_PENDANT_OTA_CONFIRM_S * 1000LL) {
		LOG_INF("confirming the new image: %d", boot_write_img_confirmed());
	}
}
