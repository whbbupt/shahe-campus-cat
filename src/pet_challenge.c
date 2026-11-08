#include "pet_challenge.h"
#include <string.h>

static void result(pg_game_t *g, bool won)
{
    g->phase = won ? PG_WON : PG_LOST;
    g->elapsed = 0;
}
void pg_start(pg_game_t *g, pg_kind_t kind, uint32_t seed)
{
    memset(g, 0, sizeof(*g));
    g->kind = kind;
    if (kind < PG_MEAL || kind > PG_FIND) return;
    g->phase = kind == PG_FIND ? PG_SHOW : PG_INPUT;
    for (unsigned i = 0; i < 4; i++) g->cards[i] = (uint8_t)i;
    for (unsigned i = 3; i > 0; i--)
    {
        seed = seed * 1664525U + 1013904223U;
        unsigned j = (seed >> 16) % (i + 1);
        uint8_t old = g->cards[i]; g->cards[i] = g->cards[j]; g->cards[j] = old;
    }
    for (unsigned i = 0; i < 3; i++) g->targets[i] = g->cards[(i + 1) % 4];
}
bool pg_window(const pg_game_t *g)
{
    return g->kind == PG_RHYTHM && g->phase == PG_INPUT &&
        g->elapsed % 3000 >= 1100 && g->elapsed % 3000 <= 1900;
}
uint32_t pg_remaining(const pg_game_t *g)
{
    if (g->phase == PG_SHOW) return 5000 - g->elapsed;
    if (g->phase == PG_WON || g->phase == PG_LOST) return 1500 - g->elapsed;
    if (g->phase == PG_INPUT)
        return g->kind == PG_RHYTHM ? 3000 - g->elapsed % 3000 : 30000 - g->total;
    return UINT32_MAX;
}
void pg_advance(pg_game_t *g, uint32_t ms)
{
    while (ms && g->phase != PG_OFF)
    {
        uint32_t until = pg_remaining(g), dt = ms < until ? ms : until;
        g->elapsed += dt;
        g->total += dt;
        ms -= dt;
        if (dt < until) continue;
        if (g->phase == PG_SHOW) { g->phase = PG_INPUT; g->elapsed = 0; }
        else if (g->phase == PG_WON || g->phase == PG_LOST) g->phase = PG_OFF;
        else if (g->kind == PG_RHYTHM)
        {
            g->round++;
            g->pressed = 0;
            if (g->round == 6) result(g, g->score >= 4);
        }
        else result(g, false);
    }
}
void pg_press(pg_game_t *g, unsigned choice)
{
    if (g->phase != PG_INPUT || choice >= 4) return;
    if (g->kind == PG_RHYTHM)
    {
        if (choice != 0 || g->pressed) return;
        g->pressed = 1;
        if (pg_window(g)) g->score++;
        return;
    }
    unsigned value = g->kind == PG_FIND ? g->cards[choice] : choice;
    if (value != g->targets[g->round]) { result(g, false); return; }
    g->score++;
    if (++g->round == 3) result(g, true);
}
