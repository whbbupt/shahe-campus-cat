#ifndef PET_KEY2_H
#define PET_KEY2_H
#include <stdbool.h>
#include <stdint.h>

typedef enum { PET_KEY2_NONE, PET_KEY2_SHORT, PET_KEY2_LONG } pet_key2_event_t;
typedef struct {
    uint32_t changed_at, pressed_at;
    bool raw, down, long_sent;
} pet_key2_t;

void pet_key2_init(pet_key2_t *key, bool pressed, uint32_t now);
pet_key2_event_t pet_key2_sample(pet_key2_t *key, bool pressed, uint32_t now);
bool pet_key2_selftest(void);
#endif
