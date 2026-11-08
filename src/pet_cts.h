#ifndef PET_CTS_H
#define PET_CTS_H
#include <stdbool.h>
#include <stdint.h>
typedef bool (*pet_cts_submit_t)(uint32_t utc,uint32_t received_tick);
void pet_cts_init(pet_cts_submit_t submit);
/* Worker-owned SDK requests. No extra thread, UI, RTC or Flash. */
void pet_cts_connected(uint8_t connection);
void pet_cts_disconnected(uint8_t connection);
void pet_cts_poll(void);
/* SDK callbacks copy bounded values; invalidate immediately on disconnect. */
void pet_cts_invalidate(uint8_t connection);
void pet_cts_event(uint16_t id,uint8_t *data,uint16_t length);
unsigned pet_cts_status(void); /* 0 idle, 1 working, 2 unavailable, 3 sample queued */
const char *pet_cts_stage(void);
#endif
