#ifndef PET_AUDIO_H
#define PET_AUDIO_H
#include "pet_world.h"
#include <stddef.h>
typedef enum {
    PA_NONE, PA_TOUCH, PA_MEAL, PA_REST, PA_DONE, PA_STAMP, PA_WIN, PA_RETRY,
    PA_NOTE0, PA_NOTE1, PA_NOTE2, PA_NOTE3, PA_CUE_COUNT
} pa_cue_t;
typedef struct {
    uint32_t settlements[PW_ACTION_COUNT];
    uint16_t stamps, wins;
    pw_phase_t phase;
    pw_order_t order;
    int show_step;
    pm_phase_t game_phase;
    pg_phase_t challenge_phase;
    uint8_t challenge_score;
    uint8_t challenge_round;
    uint32_t snacks;
    uint8_t furniture_owned, furniture_equipped;
    uint16_t coins;
} pa_observer_t;
typedef struct {
    uint32_t music_at, cue_at;
    pa_cue_t cue;
} pa_mixer_t;
void pa_observer_init(pa_observer_t *, const pet_world_t *);
pa_cue_t pa_observe(pa_observer_t *, const pet_world_t *);
void pa_trigger(pa_mixer_t *, pa_cue_t);
void pa_mix(pa_mixer_t *, unsigned mode, bool duck, int16_t *, size_t);
bool pa_selftest(void (*report)(const char *, bool));
#endif
