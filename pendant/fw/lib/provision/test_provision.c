#include <assert.h>
#include <stdio.h>
#include "provision.h"

int main(void)
{
	struct provision p;

	/* 1. No keys: no recording. Bond/time events cannot skip initialization, and bond loss
	 * while uninitialized must not create a path to READY. */
	provision_init(&p, 0, 0, 0);
	assert(p.state == PROV_UNINITIALIZED && !provision_record_allowed(&p));
	provision_bonded(&p, 0);
	provision_bonded(&p, 1);
	assert(p.state == PROV_UNINITIALIZED && !provision_record_allowed(&p));
	assert(!provision_time_set(&p));
	provision_time_lost(&p);
	assert(p.state == PROV_UNINITIALIZED);
	provision_bonded(&p, 0);
	assert(p.state == PROV_UNINITIALIZED);

	/* 2. Keys created: pairing wait, then TIME, then recording. */
	provision_keys_ready(&p);
	assert(p.state == PROV_PAIRING);
	assert(!provision_time_set(&p)); /* TIME before a bond is ignored */
	assert(p.state == PROV_PAIRING && !provision_record_allowed(&p));
	provision_bonded(&p, 1);
	assert(p.state == PROV_TIME && !provision_record_allowed(&p));
	assert(provision_time_set(&p));
	assert(p.state == PROV_READY && provision_record_allowed(&p));

	/* 3. Losing the bond drops to PAIRING and clears the time requirement. */
	provision_bonded(&p, 0);
	assert(p.state == PROV_PAIRING && !provision_record_allowed(&p));
	assert(!provision_time_set(&p));
	provision_bonded(&p, 1);
	assert(p.state == PROV_TIME);
	assert(provision_time_set(&p));
	assert(p.state == PROV_READY);

	/* 4. RTC loss goes back to waiting for TIME. */
	provision_time_lost(&p);
	assert(p.state == PROV_TIME && !provision_record_allowed(&p));
	assert(provision_time_set(&p));
	assert(p.state == PROV_READY);

	/* 5. Key loss is fail-closed and cannot be repaired by bond/time alone. */
	provision_keys_lost(&p);
	assert(p.state == PROV_UNINITIALIZED && !provision_record_allowed(&p));
	provision_bonded(&p, 1);
	assert(!provision_time_set(&p));
	assert(p.state == PROV_UNINITIALIZED);
	provision_keys_ready(&p);
	assert(p.state == PROV_TIME); /* the bond survived; TIME must be sent again */

	/* 6. A bond that appears before the key store is not forgotten, but a trusted clock
	 * before the key store does not skip the post-key TIME step. */
	provision_init(&p, 0, 1, 1);
	assert(p.state == PROV_UNINITIALIZED);
	provision_keys_ready(&p);
	assert(p.state == PROV_TIME && !provision_record_allowed(&p));
	assert(provision_time_set(&p) && p.state == PROV_READY);

	/* 7. Reboot with an existing bond: TIME is still required. */
	provision_init(&p, 1, 1, 0);
	assert(p.state == PROV_TIME && !provision_record_allowed(&p));
	assert(provision_time_set(&p));
	assert(p.state == PROV_READY);

	/* 8. Reboot with bond + trusted time: ready immediately. */
	provision_init(&p, 1, 1, 1);
	assert(p.state == PROV_READY && provision_record_allowed(&p));

	/* 9. Rebooting without a bond never records, even if the clock was set. A later bond
	 * still has to wait for TIME. */
	provision_init(&p, 1, 0, 1);
	assert(p.state == PROV_PAIRING && !provision_record_allowed(&p));
	provision_bonded(&p, 1);
	assert(p.state == PROV_TIME && !provision_record_allowed(&p));
	assert(provision_time_set(&p) && p.state == PROV_READY);

	puts("provision: all tests passed");
	return 0;
}
