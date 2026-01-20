

#ifndef __FREERTOS_DEMO_H
#define __FREERTOS_DEMO_H

#include <stdint.h>

void freertos_demo(void);
void PRE_SLEEP_PROCESSING(void);
void POST_SLEEP_PROCESSING(void);

/* ---------------- Watchdog supervisor (heartbeat) ----------------
 * Used to avoid "fake hang": if key tasks stop running, feed tasks stop
 * feeding watchdogs so the MCU resets.
 */
#ifndef WDG_SUPERVISOR_ENABLE
#define WDG_SUPERVISOR_ENABLE 1
#endif

#ifndef WDG_SUPERVISOR_GRACE_MS
#define WDG_SUPERVISOR_GRACE_MS 15000
#endif

#ifndef WDG_SUPERVISOR_MAC_TIMEOUT_MS
#define WDG_SUPERVISOR_MAC_TIMEOUT_MS 5000
#endif

#ifndef WDG_SUPERVISOR_ROUTE_TIMEOUT_MS
#define WDG_SUPERVISOR_ROUTE_TIMEOUT_MS 5000
#endif

#define WDG_HB_ID_MAC    0
#define WDG_HB_ID_ROUTE  1
#define WDG_HB_ID_MAX    2

void WDG_Heartbeat(uint8_t id);


#endif

