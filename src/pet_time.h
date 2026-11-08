#ifndef PET_TIME_H
#define PET_TIME_H
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
typedef struct { unsigned year, month, day, hour, minute, second; } pt_date_t;
typedef enum { PT_WAIT, PT_RTC, PT_SYNCED, PT_ERROR } pt_clock_state_t;
typedef enum { PT_NO_REQUEST, PT_OK, PT_IO_ERROR, PT_VERIFY_ERROR } pt_result_t;
typedef struct {
    uint32_t utc;
    bool valid, trusted;
    pt_clock_state_t state;
    pt_result_t result;
} pt_clock_t;
typedef enum { PT_MORNING, PT_DAY, PT_DUSK, PT_NIGHT } pt_band_t;
bool pt_encode(const pt_date_t *date, uint32_t *utc);
bool pt_decode(uint32_t utc, pt_date_t *date);
bool pt_parse(const uint8_t *bytes, size_t length, uint32_t *utc);
unsigned pt_minute(uint32_t utc);
pt_band_t pt_band(uint32_t utc);
bool pt_class_time(uint32_t utc);
bool pt_meal_time(uint32_t utc);
unsigned pt_bedtime(uint32_t utc); /* 0=normal, 1=prepare, 2=tired */
bool pt_selftest(void (*report)(const char *, bool));
#endif
