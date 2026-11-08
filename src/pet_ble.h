#ifndef PET_BLE_H
#define PET_BLE_H
#include <rtthread.h>
#include "pet_link.h"
#include "pet_time.h"
typedef struct { uint32_t utc, token; } pet_time_request_t;
/* SDK callbacks enqueue requests; only the main thread touches RTC. */
rt_err_t pet_ble_start(void);
/* Main-loop watchdog: prints one snapshot after 15 s, independent of worker. */
void pet_ble_poll(void);
void pet_ble_get_link(pet_link_t *link);
/* Main-thread publisher. SDK callbacks only copy the last complete text. */
void pet_ble_set_phone_status(const pet_phone_status_t *status);
void pet_ble_set_clock(const pt_clock_t *clock);
bool pet_ble_take_time_request(pet_time_request_t *request);
void pet_ble_finish_time_request(uint32_t token);
#endif
