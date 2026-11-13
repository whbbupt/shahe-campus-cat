#ifndef PET_CHALLENGE_H
#define PET_CHALLENGE_H
#include <stdbool.h>
#include <stdint.h>
typedef enum
{
    PG_NONE, PG_MEAL, PG_RHYTHM, PG_FIND, PG_LIFE, PG_PROJECTION, PG_CALIBRATION, PG_CIRCUIT
} pg_kind_t;
enum { PG_PROJECTION_STEPS = 6, PG_GRID_CELLS = 9, PG_GRID_MOVES = 6, PG_CIRCUIT_PORTS = 6, PG_CIRCUIT_LINKS = 3 };
typedef enum { PG_OFF, PG_SHOW, PG_INPUT, PG_WON, PG_LOST } pg_phase_t;
typedef struct
{
    pg_kind_t kind;
    pg_phase_t phase;
    uint32_t elapsed, total;
    uint8_t round, score, pressed, cards[4], targets[3];
    uint16_t grid;
    uint8_t selected, links, mistakes;
} pg_game_t;
void pg_start(pg_game_t *game, pg_kind_t kind, uint32_t seed);
void pg_advance(pg_game_t *game, uint32_t ms);
void pg_press(pg_game_t *game, unsigned choice);
uint32_t pg_remaining(const pg_game_t *game);
bool pg_window(const pg_game_t *game);
unsigned pg_projection(const pg_game_t *game, uint8_t pattern[PG_PROJECTION_STEPS], uint8_t options[3]);
unsigned pg_circuit_color(const pg_game_t *game, unsigned port);
bool pg_selftest(void (*report)(const char *, bool));
#endif
