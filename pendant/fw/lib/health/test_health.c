/* Host test: make -C fw/lib health */
#include <assert.h>
#include <stdio.h>
#include "health.h"

int main(void)
{
	struct health h;

	health_init(&h);
	assert(health_ok(&h, 100000)); /* nothing armed: nothing to wait for */

	/* Two tasks beating on time keep it healthy. */
	health_arm(&h, 0, 2000, 0);
	health_arm(&h, 1, 15000, 0);
	for (uint32_t t = 100; t < 60000; t += 100) {
		health_beat(&h, 0, t);
		if (t % 5000 == 0) {
			health_beat(&h, 1, t);
		}
		assert(health_ok(&h, t));
	}

	/* Task 0 stops beating (a deadlock): late after its timeout, and only it is blamed. */
	assert(health_ok(&h, 61900) && !h.stuck);
	assert(!health_ok(&h, 62200) && h.stuck == 1u);
	health_beat(&h, 0, 62300); /* too late to matter for the flag, but it is healthy again */
	assert(health_ok(&h, 62400) && h.stuck == 1u);

	/* Idle by design: disarmed tasks are not waited for; re-arming starts a fresh deadline. */
	health_disarm(&h, 0);
	assert(health_ok(&h, 500000 - 1) == 0 && (h.stuck & 2u)); /* task 1 stopped too: 15 s passed */
	health_beat(&h, 1, 500000);
	assert(health_ok(&h, 500000));
	health_arm(&h, 0, 2000, 500000);
	assert(health_ok(&h, 501999) && !health_ok(&h, 502001));

	/* The 32 bit millisecond counter wraps after 49 days without a false alarm. */
	health_init(&h);
	health_arm(&h, 2, 2000, 0xfffffe00u);
	assert(health_ok(&h, 0x00000100u)); /* 768 ms later, across the wrap */
	assert(!health_ok(&h, 0x00000800u));

	/* Out-of-range ids are ignored. */
	health_arm(&h, HEALTH_MAX, 10, 0);
	health_beat(&h, 99, 0);
	health_disarm(&h, 99);

	puts("health: all tests passed");
	return 0;
}
