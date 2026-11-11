#include "pet_challenge.h"
#include <string.h>

static void result(pg_game_t *g, bool won)
{
    g->phase = won ? PG_WON : PG_LOST;
    g->elapsed = 0;
}
static uint16_t grid_flip(unsigned cell)
{
    uint16_t mask = (uint16_t)(1U << cell);
    if (cell >= 3) mask |= (uint16_t)(1U << (cell - 3));
    if (cell < 6) mask |= (uint16_t)(1U << (cell + 3));
    if (cell % 3 != 0) mask |= (uint16_t)(1U << (cell - 1));
    if (cell % 3 != 2) mask |= (uint16_t)(1U << (cell + 1));
    return mask;
}

enum { GRID_LIT = (1U << PG_GRID_CELLS) - 1 };
const uint8_t pg_circuit_links[PG_CIRCUIT_LINKS][2] = {{0, 2}, {3, 4}, {5, 1}};

static unsigned circuit_link(unsigned port)
{
    for (unsigned i = 0; i < PG_CIRCUIT_LINKS; i++)
        if (port == pg_circuit_links[i][0] || port == pg_circuit_links[i][1])
            return i;
    return PG_CIRCUIT_LINKS;
}

unsigned pg_projection(const pg_game_t *g, uint8_t pattern[PG_PROJECTION_STEPS], uint8_t options[3])
{
    static const uint8_t patterns[][PG_PROJECTION_STEPS] = {
        {0, 1, 0, 1, 0, 1}, {0, 1, 2, 0, 1, 2}, {0, 0, 1, 0, 0, 1}
    };
    unsigned round = g->round % 3;
    for (unsigned i = 0; i < PG_PROJECTION_STEPS; i++)
        pattern[i] = g->cards[patterns[round][i]];
    for (unsigned i = 0; i < 3; i++)
        options[i] = g->cards[(i + round) % 3];
    return (3 - round) % 3;
}

void pg_start(pg_game_t *g, pg_kind_t kind, uint32_t seed)
{
    memset(g, 0, sizeof(*g));
    g->kind = kind;
    if (kind < PG_MEAL || kind == PG_LIFE || kind > PG_CIRCUIT) return;
    g->selected = PG_CIRCUIT_PORTS;
    g->phase = kind == PG_FIND ? PG_SHOW : PG_INPUT;
    for (unsigned i = 0; i < 4; i++) g->cards[i] = (uint8_t)i;
    for (unsigned i = 3; i > 0; i--)
    {
        seed = seed * 1664525U + 1013904223U;
        unsigned j = (seed >> 16) % (i + 1);
        uint8_t old = g->cards[i]; g->cards[i] = g->cards[j]; g->cards[j] = old;
    }
    for (unsigned i = 0; i < 3; i++) g->targets[i] = g->cards[(i + 1) % 4];
    if (kind == PG_CALIBRATION)
    {
        unsigned used = 0;
        g->grid = GRID_LIT;
        for (unsigned i = 0; i < 3; i++)
        {
            seed = seed * 1664525U + 1013904223U;
            unsigned cell = (seed >> 16) % PG_GRID_CELLS;
            while (used & (1U << cell)) cell = (cell + 1) % PG_GRID_CELLS;
            used |= 1U << cell;
            g->grid ^= grid_flip(cell);
        }
    }
}
bool pg_window(const pg_game_t *g)
{
    return g->kind == PG_RHYTHM && g->phase == PG_INPUT &&
        g->elapsed % 3000 >= 1100 && g->elapsed % 3000 <= 1900;
}
uint32_t pg_remaining(const pg_game_t *g)
{
    if (g->phase == PG_SHOW) return 5000 - g->elapsed;
    if (g->phase == PG_WON || g->phase == PG_LOST)
        return (g->kind >= PG_PROJECTION ? 2500U : 1500U) - g->elapsed;
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
static void press_circuit(pg_game_t *g, unsigned port)
{
    if (port >= PG_CIRCUIT_PORTS) return;
    unsigned link = circuit_link(port);
    if (g->links & (1U << link)) return;
    unsigned first = g->selected;
    g->selected = PG_CIRCUIT_PORTS;
    if (first == port) return;
    if (first == PG_CIRCUIT_PORTS)
    {
        g->selected = (uint8_t)port;
        return;
    }
    if (circuit_link(first) != link)
    {
        if (++g->mistakes == 3) result(g, false);
        return;
    }
    g->links |= (uint8_t)(1U << link);
    if (++g->score == PG_CIRCUIT_LINKS) result(g, true);
}

void pg_press(pg_game_t *g, unsigned choice)
{
    if (g->phase != PG_INPUT) return;
    if (g->kind == PG_CIRCUIT)
    {
        press_circuit(g, choice);
        return;
    }
    if (g->kind == PG_CALIBRATION)
    {
        if (choice >= PG_GRID_CELLS) return;
        g->grid ^= grid_flip(choice);
        g->round++;
        if (g->grid == GRID_LIT || g->round == PG_GRID_MOVES)
            result(g, g->grid == GRID_LIT);
        return;
    }
    if (g->kind == PG_PROJECTION)
    {
        if (choice >= 3) return;
        uint8_t pattern[PG_PROJECTION_STEPS], options[3];
        if (choice == pg_projection(g, pattern, options)) g->score++;
        if (++g->round == 3) result(g, g->score >= 2);
        return;
    }
    if (choice >= 4) return;
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
