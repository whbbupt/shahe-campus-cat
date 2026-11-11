#include "pet_challenge.h"

static bool projection_challenge(void)
{
    pg_game_t game;
    pg_start(&game, PG_PROJECTION, 19);
    for (unsigned i = 0; i < 3; i++)
    {
        uint8_t pattern[PG_PROJECTION_STEPS], options[3];
        unsigned correct = pg_projection(&game, pattern, options);
        if (correct >= 3 || options[correct] != pattern[0] || options[0] == options[1] ||
            options[1] == options[2] || options[0] == options[2]) return false;
        pg_press(&game, i == 1 ? (correct + 1) % 3 : correct);
    }
    if (game.phase != PG_WON || game.score != 2) return false;
    pg_press(&game, 0);
    return game.phase == PG_WON && game.score == 2;
}

static bool calibration_challenge(void)
{
    static const unsigned flips[] = {11, 23, 38, 89, 186, 308, 200, 464, 416};
    for (unsigned seed = 0; seed < 16; seed++)
    {
        pg_game_t game;
        pg_start(&game, PG_CALIBRATION, seed);
        unsigned solution = 0;
        for (unsigned candidate = 1; candidate < 512; candidate++)
        {
            unsigned grid = game.grid;
            for (unsigned i = 0; i < PG_GRID_CELLS; i++)
                if (candidate & (1U << i)) grid ^= flips[i];
            if (grid == 511)
            {
                solution = candidate;
                break;
            }
        }
        if (!solution) return false;
        for (unsigned i = 0; i < PG_GRID_CELLS; i++)
            if (solution & (1U << i)) pg_press(&game, i);
        if (game.phase != PG_WON || game.round != 3 || game.grid != 511) return false;
    }
    pg_game_t game;
    pg_start(&game, PG_CALIBRATION, 19);
    for (unsigned i = 0; i < PG_GRID_MOVES; i++) pg_press(&game, 0);
    return game.phase == PG_LOST && game.round == PG_GRID_MOVES;
}

static bool circuit_challenge(void)
{
    pg_game_t game;
    pg_start(&game, PG_CIRCUIT, 0);
    pg_press(&game, 1);
    pg_press(&game, 1);
    if (game.selected != PG_CIRCUIT_PORTS || game.mistakes) return false;
    pg_press(&game, 0);
    pg_press(&game, 1);
    if (game.mistakes != 1 || game.links) return false;
    const unsigned ports[] = {2, 0, 1, 5, 4, 3};
    for (unsigned i = 0; i < sizeof(ports) / sizeof(ports[0]); i++) pg_press(&game, ports[i]);
    if (game.phase != PG_WON || game.links != 7 || game.score != 3) return false;
    pg_start(&game, PG_CIRCUIT, 0);
    for (unsigned i = 0; i < 3; i++)
    {
        pg_press(&game, 0);
        pg_press(&game, 1);
    }
    return game.phase == PG_LOST && game.mistakes == 3 && !game.links;
}

bool pg_selftest(void (*report)(const char *, bool))
{
    bool all = true;
    for (pg_kind_t kind = PG_MEAL; kind <= PG_FIND; kind++)
    {
        pg_game_t g;
        pg_start(&g, kind, 977);
        if (kind == PG_FIND) pg_advance(&g, 5000);
        if (kind == PG_RHYTHM)
        {
            for (unsigned i = 0; i < 6; i++)
            {
                pg_advance(&g, 1500);
                pg_press(&g, 0); pg_press(&g, 0);
                pg_advance(&g, 1500);
            }
        }
        else for (unsigned i = 0; i < 3; i++)
        {
            unsigned pick = g.targets[i];
            if (kind == PG_FIND)
                for (unsigned j = 0; j < 4; j++) if (g.cards[j] == g.targets[i]) pick = j;
            pg_press(&g, pick);
        }
        bool ok = g.phase == PG_WON && g.score == (kind == PG_RHYTHM ? 6 : 3);
        pg_press(&g, 0);
        ok = ok && g.phase == PG_WON;
        pg_advance(&g, 1500);
        ok = ok && g.phase == PG_OFF;
        pg_start(&g, kind, 123);
        pg_advance(&g, 31500);
        ok = ok && g.phase == PG_OFF;
        if (report) report(kind == PG_MEAL ? "meal-challenge" : kind == PG_RHYTHM ? "rhythm-challenge" : "find-challenge", ok);
        all = all && ok;
    }
    static const struct
    {
        pg_kind_t kind;
        const char *name;
        bool (*run)(void);
    } teaching[] = {
        {PG_PROJECTION, "projection-rules", projection_challenge},
        {PG_CALIBRATION, "grid-solvability-and-move-limit", calibration_challenge},
        {PG_CIRCUIT, "circuit-ports-and-mistake-limit", circuit_challenge}
    };
    for (unsigned i = 0; i < sizeof(teaching) / sizeof(teaching[0]); i++)
    {
        pg_game_t game;
        pg_start(&game, teaching[i].kind, 19);
        pg_advance(&game, 29999);
        bool ok = game.phase == PG_INPUT && pg_remaining(&game) == 1;
        pg_advance(&game, 1);
        ok = ok && game.phase == PG_LOST && teaching[i].run();
        if (report) report(teaching[i].name, ok);
        all = all && ok;
    }
    return all;
}
