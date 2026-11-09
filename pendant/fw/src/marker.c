#include "marker.h"

static const struct gpio_dt_spec pin[M_COUNT] = {
	[M_REC] = GPIO_DT_SPEC_GET(DT_PATH(zephyr_user), marker_rec_gpios),
	[M_STORE] = GPIO_DT_SPEC_GET(DT_PATH(zephyr_user), marker_store_gpios),
	[M_BLE] = GPIO_DT_SPEC_GET(DT_PATH(zephyr_user), marker_ble_gpios),
};

void marker_init(void)
{
	for (int i = 0; i < M_COUNT; i++) {
		gpio_pin_configure_dt(&pin[i], GPIO_OUTPUT_INACTIVE);
	}
}

void marker_set(enum marker m, int on)
{
	gpio_pin_set_dt(&pin[m], on);
}
