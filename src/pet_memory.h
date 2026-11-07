#ifndef PET_MEMORY_H
#define PET_MEMORY_H
#include <stdint.h>

typedef enum { PM_OFF, PM_SHOW, PM_INPUT, PM_WON, PM_LOST } pm_phase_t;
typedef struct
{
    pm_phase_t phase;
    uint8_t round, length, input_at, sequence[5];
    uint32_t elapsed, seed;
} pm_game_t;
void pm_start(pm_game_t *game, uint32_t seed);
void pm_advance(pm_game_t *game, uint32_t ms);
void pm_press(pm_game_t *game, unsigned symbol);
int pm_highlight(const pm_game_t *game);
uint32_t pm_remaining(const pm_game_t *game);
#endif
