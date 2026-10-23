#ifndef PET_MODEL_H
#define PET_MODEL_H

#include <stdbool.h>
#include <stdint.h>

typedef enum { PET_FEED, PET_TOUCH, PET_REST } pet_event_t;
typedef enum { PET_OK, PET_BUSY, PET_COOLDOWN, PET_FULL, PET_ASLEEP, PET_INVALID } pet_result_t;
typedef enum { PET_IDLE, PET_HAPPY, PET_HUNGRY, PET_LONELY, PET_FEEDING, PET_PETTING, PET_SLEEPING } pet_view_t;

typedef struct
{
    uint8_t food;
    uint8_t mood;
    bool sleeping;
    pet_view_t action;
    uint32_t action_ms;
    uint32_t feed_cooldown_ms;
    uint32_t touch_cooldown_ms;
    uint32_t food_elapsed_ms;
    uint32_t mood_elapsed_ms;
    uint32_t rest_elapsed_ms;
} pet_model_t;

void pet_model_init(pet_model_t *pet);
void pet_model_advance(pet_model_t *pet, uint32_t elapsed_ms);
pet_result_t pet_model_event(pet_model_t *pet, pet_event_t event);
pet_view_t pet_model_view(const pet_model_t *pet);
/* Unsigned subtraction supports one tick wrap; caller must poll within that period. */
uint32_t pet_clock_elapsed(uint32_t previous, uint32_t now, uint32_t tick_hz, uint32_t *fraction);

#endif
