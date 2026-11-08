#ifndef PET_BLE_H
#define PET_BLE_H
#include <rtthread.h>
#include "pet_link.h"
/* Asynchronous connection probe. No gameplay or RTC commands exposed. */
rt_err_t pet_ble_start(void);
/* Main-loop watchdog: prints one snapshot after 15 s, independent of worker. */
void pet_ble_poll(void);
void pet_ble_get_link(pet_link_t *link);
/* Main-thread publisher. SDK callbacks only copy the last complete text. */
void pet_ble_set_phone_status(const pet_phone_status_t *status);
#endif
