/* OTA over BLE: MCUmgr/SMP uploads into slot 1, MCUboot swaps, the new image is confirmed only after
 * it has run healthily for a while (otherwise MCUboot reverts at the next reset). Issue #11. */
#ifndef OTA_H
#define OTA_H

void ota_init(void); /* registers the upload gate, logs the running version */
void ota_poll(void); /* call about every 5 s from the main loop: confirms a test image when healthy */

#endif
