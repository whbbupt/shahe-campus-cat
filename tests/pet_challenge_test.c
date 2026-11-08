#include "pet_challenge.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static void report(const char *name, bool ok) { printf("[host:challenge] %s %s\n", name, ok ? "PASS" : "FAIL"); }
int main(void)
{
    assert(pg_selftest(report));
    for (unsigned seed = 0; seed < 32; seed++)
        for (pg_kind_t kind = PG_MEAL; kind <= PG_FIND; kind++)
        {
            pg_game_t g, a, b;
            pg_start(&g, kind, seed);
            unsigned mask = 0;
            for (unsigned i = 0; i < 4; i++) mask |= 1U << g.cards[i];
            assert(mask == 15);
            pg_press(&g, 4); assert(g.score == 0);
            if (kind == PG_FIND)
            {
                pg_press(&g, 0); assert(g.phase == PG_SHOW && !g.score);
                pg_advance(&g, 4999); assert(g.phase == PG_SHOW);
                pg_advance(&g, 1); assert(g.phase == PG_INPUT && g.total == 5000);
            }
            if (kind != PG_RHYTHM)
            {
                unsigned wrong = (g.targets[0] + 1) % 4;
                if (kind == PG_FIND) for (unsigned i = 0; i < 4; i++)
                    if (g.cards[i] == (g.targets[0] + 1) % 4) wrong = i;
                pg_press(&g, wrong); assert(g.phase == PG_LOST && !g.score);
                pg_press(&g, 0); assert(g.phase == PG_LOST);
            }
            pg_start(&a, kind, seed); b = a;
            pg_advance(&a, 31500);
            for (unsigned i = 0; i < 315; i++) pg_advance(&b, 100);
            assert(!memcmp(&a, &b, sizeof(a)) && a.phase == PG_OFF && !a.score);
        }
    for (unsigned boundary = 0; boundary < 4; boundary++)
    {
        const unsigned at[] = {1099, 1100, 1900, 1901};
        pg_game_t g; pg_start(&g, PG_RHYTHM, 0);
        pg_advance(&g, at[boundary]); pg_press(&g, 0); pg_press(&g, 0);
        assert(g.score == (boundary == 1 || boundary == 2 ? 1 : 0));
        pg_advance(&g, 3000 - at[boundary]); assert(g.round == 1 && !g.pressed);
    }
    for (unsigned hits = 3; hits <= 4; hits++)
    {
        pg_game_t g; pg_start(&g, PG_RHYTHM, 0);
        for (unsigned i = 0; i < 6; i++)
        {
            pg_advance(&g, 1500);
            if (i < hits) pg_press(&g, 0);
            pg_advance(&g, 1500);
        }
        assert(g.phase == (hits == 4 ? PG_WON : PG_LOST));
    }
    puts("[host:challenge] 32 seeds, permutation, reveal, wrong/illegal/repeat, timeout partition, exact rhythm window and four-hit threshold PASS");
    return 0;
}
