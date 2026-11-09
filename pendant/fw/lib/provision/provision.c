#include "provision.h"

static void update(struct provision *p)
{
	if (!p->keys) {
		p->state = PROV_UNINITIALIZED;
	} else if (!p->bonded) {
		p->state = PROV_PAIRING;
	} else if (!p->time) {
		p->state = PROV_TIME;
	} else {
		p->state = PROV_READY;
	}
}

void provision_init(struct provision *p, int keys_ready, int bonded, int time_valid)
{
	p->keys = !!keys_ready;
	p->bonded = !!bonded;
	p->time = !!time_valid && p->keys && p->bonded; /* TIME is only trusted after keys and a bond */
	update(p);
}

void provision_keys_ready(struct provision *p)
{
	p->keys = 1;
	update(p);
}

void provision_bonded(struct provision *p, int bonded)
{
	p->bonded = !!bonded;
	if (!p->bonded) {
		p->time = 0; /* a new bond must get TIME again */
	}
	update(p);
}

int provision_time_set(struct provision *p)
{
	if (!p->keys || !p->bonded) {
		return 0;
	}
	p->time = 1;
	update(p);
	return 1;
}

void provision_time_lost(struct provision *p)
{
	p->time = 0;
	update(p);
}

void provision_keys_lost(struct provision *p)
{
	p->keys = 0;
	p->time = 0;
	update(p);
}

int provision_record_allowed(const struct provision *p)
{
	return p->state == PROV_READY;
}

enum provision_state provision_state(const struct provision *p)
{
	return p->state;
}
