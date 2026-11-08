#ifndef PET_CHALLENGE_H
#define PET_CHALLENGE_H
#include <stdbool.h>
#include <stdint.h>
typedef enum { PG_NONE, PG_MEAL, PG_RHYTHM, PG_FIND, PG_LIFE } pg_kind_t;
typedef enum { PG_OFF, PG_SHOW, PG_INPUT, PG_WON, PG_LOST } pg_phase_t;
typedef struct
{
    pg_kind_t kind;
    pg_phase_t phase;
    uint32_t elapsed, total;
    uint8_t round, score, pressed, cards[4], targets[3];
} pg_game_t;
void pg_start(pg_game_t *game, pg_kind_t kind, uint32_t seed);
void pg_advance(pg_game_t *game, uint32_t ms);
void pg_press(pg_game_t *game, unsigned choice);
uint32_t pg_remaining(const pg_game_t *game);
bool pg_window(const pg_game_t *game);
bool pg_selftest(void (*report)(const char *, bool));
#endif
