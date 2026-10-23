#include "pet_model.h"
#include <limits.h>
#include <string.h>

static uint32_t countdown(uint32_t remaining, uint32_t elapsed)
{
    return elapsed >= remaining ? 0U : remaining - elapsed;
}

static uint8_t decrease(uint8_t value, uint64_t amount)
{
    return amount >= value ? 0U : (uint8_t)(value - amount);
}

static uint8_t increase(uint8_t value, uint64_t amount)
{
    return amount >= (uint64_t)(100U - value) ? 100U : (uint8_t)(value + amount);
}

static uint64_t periods(uint32_t *remainder, uint32_t elapsed, uint32_t interval)
{
    uint64_t total = (uint64_t)*remainder + elapsed;
    *remainder = (uint32_t)(total % interval);
    return total / interval;
}

void pet_model_init(pet_model_t *pet)
{
    memset(pet, 0, sizeof(*pet));
    pet->food = 70;
    pet->mood = 60;
    pet->action = PET_IDLE;
}

void pet_model_advance(pet_model_t *pet, uint32_t elapsed_ms)
{
    pet->feed_cooldown_ms = countdown(pet->feed_cooldown_ms, elapsed_ms);
    pet->touch_cooldown_ms = countdown(pet->touch_cooldown_ms, elapsed_ms);
    pet->action_ms = countdown(pet->action_ms, elapsed_ms);
    if (!pet->action_ms)
        pet->action = PET_IDLE;

    pet->food = decrease(pet->food, periods(&pet->food_elapsed_ms, elapsed_ms, 300000U));
    if (pet->sleeping)
        pet->mood = increase(pet->mood, periods(&pet->rest_elapsed_ms, elapsed_ms, 60000U));
    else
        pet->mood = decrease(pet->mood, periods(&pet->mood_elapsed_ms, elapsed_ms, 600000U));
}

pet_result_t pet_model_event(pet_model_t *pet, pet_event_t event)
{
    if (event != PET_FEED && event != PET_TOUCH && event != PET_REST)
        return PET_INVALID;
    if (pet->action_ms)
        return PET_BUSY;
    if (event == PET_REST)
    {
        pet->sleeping = !pet->sleeping;
        pet->rest_elapsed_ms = 0;
        return PET_OK;
    }
    if (pet->sleeping)
        return PET_ASLEEP;
    if (event == PET_FEED)
    {
        if (pet->food == 100)
            return PET_FULL;
        if (pet->feed_cooldown_ms)
            return PET_COOLDOWN;
        pet->food = increase(pet->food, 15);
        pet->feed_cooldown_ms = 2000;
        pet->action = PET_FEEDING;
        pet->action_ms = 1200;
    }
    else
    {
        if (pet->mood == 100)
            return PET_FULL;
        if (pet->touch_cooldown_ms)
            return PET_COOLDOWN;
        pet->mood = increase(pet->mood, 10);
        pet->touch_cooldown_ms = 3000;
        pet->action = PET_PETTING;
        pet->action_ms = 900;
    }
    return PET_OK;
}

pet_view_t pet_model_view(const pet_model_t *pet)
{
    if (pet->action_ms)
        return pet->action;
    if (pet->sleeping)
        return PET_SLEEPING;
    if (pet->food < 30)
        return PET_HUNGRY;
    if (pet->mood < 30)
        return PET_LONELY;
    return pet->mood >= 80 ? PET_HAPPY : PET_IDLE;
}

uint32_t pet_clock_elapsed(uint32_t previous, uint32_t now, uint32_t tick_hz, uint32_t *fraction)
{
    if (!tick_hz)
        return 0;
    uint64_t scaled = (uint64_t)(uint32_t)(now - previous) * 1000U + *fraction;
    *fraction = (uint32_t)(scaled % tick_hz);
    uint64_t elapsed = scaled / tick_hz;
    return elapsed > UINT32_MAX ? UINT32_MAX : (uint32_t)elapsed;
}
