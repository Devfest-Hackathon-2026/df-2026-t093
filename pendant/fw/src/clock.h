/* Wall clock for record timestamps. Phase 0 has no RTC chip: the phone sets it over BLE
 * (op TIME) and it counts from uptime. Until then records carry RTC_INVALID (docs/spec/formats.md §2, docs/spec/firmware.md §5.2). */
#ifndef CLOCK_H
#define CLOCK_H

#include <stdint.h>

/* unix ms now, or 0 if never set. */
uint64_t clock_rtc_ms(void);
int clock_time_valid(uint64_t unix_ms);
int clock_set(uint64_t unix_ms); /* 0 on success, -EINVAL outside 2000..2100 */

#endif
