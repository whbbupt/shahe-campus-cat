#include "pet_challenge.h"

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
    return all;
}
