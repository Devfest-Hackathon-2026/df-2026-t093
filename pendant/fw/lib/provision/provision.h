/* Fail-closed startup gate: no recording until keys exist, a bond exists, and TIME has been set.
 * Pure C; the BLE/key-store glue lives in the application (issue #37). */
#ifndef PROVISION_H
#define PROVISION_H

#include <stdint.h>

enum provision_state {
	PROV_UNINITIALIZED, /* no device keys yet */
	PROV_PAIRING,       /* advertising, waiting for the first bond */
	PROV_TIME,          /* bonded, waiting for the phone's TIME */
	PROV_READY,         /* bonded + time: recording is allowed */
};

struct provision {
	enum provision_state state;
	int keys, bonded, time;
};

/* keys_ready: device keys were loaded/created. bonded: a bond existed at boot.
 * time_valid: the RTC/clock has a trusted time. */
void provision_init(struct provision *p, int keys_ready, int bonded, int time_valid);
void provision_keys_ready(struct provision *p);
/* Bond created / lost. Losing a bond clears the time requirement, so a new bond must set TIME. */
void provision_bonded(struct provision *p, int bonded);
/* Returns 1 if TIME was accepted; 0 while uninitialized or unbonded. */
int provision_time_set(struct provision *p);
/* RTC lost its time (PORF/VLF): READY goes back to TIME. */
void provision_time_lost(struct provision *p);
/* Key store erased/corrupt: always back to UNINITIALIZED. */
void provision_keys_lost(struct provision *p);
int provision_record_allowed(const struct provision *p);
enum provision_state provision_state(const struct provision *p);

#endif
