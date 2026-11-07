#ifndef PET_BLE_PROBE_H
#define PET_BLE_PROBE_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
/* Test-only protocol: no pet state, RTC or storage access. */
bool pet_ble_probe_reply(const uint8_t *input, size_t length, uint8_t reply[4]);
bool pet_ble_probe_selftest(void);
#endif
