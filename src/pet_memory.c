#include "pet_memory.h"
#include <string.h>

static uint32_t beat_ms(const pm_game_t *game)
{
    return 650U - 50U * game->round;
}

static void show_round(pm_game_t *game)
{
    game->length = (uint8_t)(4 + game->round);
    game->input_at = 0;
    game->elapsed = 0;
    game->phase = PM_SHOW;
    for (unsigned i = 0; i < game->length; i++)
    {
        game->seed = game->seed * 1664525U + 1013904223U;
        game->sequence[i] = (uint8_t)((game->seed >> 16) % 4);
    }
}
void pm_start(pm_game_t *game, uint32_t seed)
{
    memset(game, 0, sizeof(*game));
    game->seed = seed;
    show_round(game);
}
uint32_t pm_remaining(const pm_game_t *game)
{
    if (game->phase == PM_SHOW)
        return game->length * beat_ms(game) - game->elapsed;
    if (game->phase == PM_INPUT)
        return PM_INPUT_MS - game->elapsed;
    if (game->phase == PM_WON || game->phase == PM_LOST)
        return 1500U - game->elapsed;
    return UINT32_MAX;
}
void pm_advance(pm_game_t *game, uint32_t ms)
{
    while (ms && game->phase != PM_OFF)
    {
        uint32_t remaining = pm_remaining(game);
        uint32_t dt = ms < remaining ? ms : remaining;
        game->elapsed += dt;
        ms -= dt;
        if (dt < remaining)
            break;
        game->elapsed = 0;
        if (game->phase == PM_SHOW)
            game->phase = PM_INPUT;
        else if (game->phase == PM_INPUT)
            game->phase = PM_LOST;
        else
            game->phase = PM_OFF;
    }
}
void pm_press(pm_game_t *game, unsigned symbol)
{
    if (game->phase != PM_INPUT || symbol >= 4)
        return;
    if (symbol != game->sequence[game->input_at])
    {
        game->phase = PM_LOST;
        game->elapsed = 0;
        return;
    }
    game->input_at++;
    if (game->input_at != game->length)
        return;
    if (game->round + 1 == PM_ROUNDS)
    {
        game->phase = PM_WON;
        game->elapsed = 0;
        return;
    }
    game->round++;
    show_round(game);
}
int pm_highlight(const pm_game_t *game)
{
    if (game->phase != PM_SHOW)
        return -1;
    uint32_t beat = beat_ms(game);
    if (game->elapsed % beat >= beat * 2 / 3)
        return -1;
    return game->sequence[game->elapsed / beat];
}
