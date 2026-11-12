#include "pet_key2.h"

enum { DEBOUNCE_MS = 30, LONG_PRESS_MS = 700 };

void pet_key2_init(pet_key2_t *key, bool pressed, uint32_t now)
{
    /* A key held during boot is ignored until it has been released. */
    *key = (pet_key2_t){.changed_at = now, .pressed_at = now,
                        .raw = pressed, .down = pressed, .long_sent = pressed};
}

pet_key2_event_t pet_key2_sample(pet_key2_t *key, bool pressed, uint32_t now)
{
    if (pressed != key->raw)
    {
        key->raw = pressed;
        key->changed_at = now;
    }
    if (now - key->changed_at < DEBOUNCE_MS) return PET_KEY2_NONE;
    if (key->down != key->raw)
    {
        key->down = key->raw;
        if (!key->down)
        {
            if (key->long_sent) return PET_KEY2_NONE;
            return key->changed_at - key->pressed_at >= LONG_PRESS_MS ? PET_KEY2_LONG : PET_KEY2_SHORT;
        }
        key->pressed_at = key->changed_at;
        key->long_sent = false;
    }
    if (key->down && !key->long_sent && now - key->pressed_at >= LONG_PRESS_MS)
    {
        key->long_sent = true;
        return PET_KEY2_LONG;
    }
    return PET_KEY2_NONE;
}

bool pet_key2_selftest(void)
{
    static const struct { uint16_t ms; bool down; pet_key2_event_t event; } samples[] = {
        {5, true, PET_KEY2_NONE}, {12, false, PET_KEY2_NONE}, {45, false, PET_KEY2_NONE},
        {100, true, PET_KEY2_NONE}, {130, true, PET_KEY2_NONE},
        {200, false, PET_KEY2_NONE}, {229, false, PET_KEY2_NONE}, {230, false, PET_KEY2_SHORT},
        {300, true, PET_KEY2_NONE}, {330, true, PET_KEY2_NONE}, {999, true, PET_KEY2_NONE},
        {1000, true, PET_KEY2_LONG}, {1100, true, PET_KEY2_NONE},
        {1200, false, PET_KEY2_NONE}, {1230, false, PET_KEY2_NONE},
        {1300, true, PET_KEY2_NONE}, {1330, true, PET_KEY2_NONE},
        {2001, false, PET_KEY2_NONE}, {2031, false, PET_KEY2_LONG}
    };
    pet_key2_t key;
    pet_key2_init(&key, false, 0);
    for (unsigned i = 0; i < sizeof(samples) / sizeof(samples[0]); i++)
        if (pet_key2_sample(&key, samples[i].down, samples[i].ms) != samples[i].event) return false;
    pet_key2_init(&key, true, 0);
    if (pet_key2_sample(&key, true, 900) != PET_KEY2_NONE ||
        pet_key2_sample(&key, false, 910) != PET_KEY2_NONE ||
        pet_key2_sample(&key, false, 940) != PET_KEY2_NONE) return false;
    pet_key2_init(&key, false, UINT32_MAX - 60);
    return pet_key2_sample(&key, true, UINT32_MAX - 50) == PET_KEY2_NONE &&
           pet_key2_sample(&key, true, 0) == PET_KEY2_NONE &&
           pet_key2_sample(&key, true, 649) == PET_KEY2_LONG;
}
