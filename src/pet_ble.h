#ifndef PET_BLE_H
#define PET_BLE_H
#include <rtthread.h>
/* Asynchronous connection probe. No gameplay or RTC commands exposed. */
rt_err_t pet_ble_start(void);
#endif
