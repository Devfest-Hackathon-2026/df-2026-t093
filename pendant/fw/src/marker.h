/* Event markers for the oscilloscope (docs/plan/prototype.md §2.4). Pins come from /zephyr,user. */
#ifndef MARKER_H
#define MARKER_H

#include <zephyr/drivers/gpio.h>

enum marker { M_REC, M_STORE, M_BLE, M_COUNT };

void marker_init(void);
void marker_set(enum marker m, int on);

#endif
