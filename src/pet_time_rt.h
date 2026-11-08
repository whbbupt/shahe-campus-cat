#ifndef PET_TIME_RT_H
#define PET_TIME_RT_H
#include "pet_time.h"
void pet_time_start(bool trusted);
void pet_time_poll(uint32_t elapsed_ms);
pt_result_t pet_time_sync(uint32_t utc);
pt_clock_t pet_time_snapshot(void);
#endif
