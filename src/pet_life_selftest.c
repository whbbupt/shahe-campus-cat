#include "pet_world.h"
#include <string.h>

static void room(pet_world_t *w, pw_scene_t scene)
{
    pw_init(w); w->scene = scene;
    pw_point_t p = pw_node(scene, 3); w->x = p.x; w->y = p.y;
}
static bool arrive(pet_world_t *w)
{
    for (unsigned i = 0; i < 500 && w->phase == PW_WALK; i++)
    {
        if (!pw_room_ground(w->scene, w->x, w->y)) return false;
        pw_advance(w, 20);
    }
    return w->phase != PW_WALK;
}
static bool games(void)
{
    for (unsigned k = 0; k < 3; k++)
    {
        pet_world_t w;
        room(&w, k == 0 ? PW_CANTEEN : k == 1 ? PW_GYM : PW_DORM);
        pw_command_t command = (pw_command_t)(PW_MEAL_GAME + k);
        if (pw_request(&w, command) != PW_OK) return false;
        if (pw_request(&w, PW_MEMORY_START) != PW_WRONG_ROOM || w.challenge.kind != (pg_kind_t)(PG_MEAL + k)) return false;
        if (!arrive(&w) || w.phase != PW_MINIGAME) return false;
        if (pw_request(&w, PW_EXIT) != PW_BUSY) return false;
        if (k == 2) pw_advance(&w, 5000);
        if (k == 1)
        {
            for (unsigned i = 0; i < 6; i++)
            {
                pw_advance(&w, 1500); pw_request(&w, PW_MEMORY_0); pw_request(&w, PW_MEMORY_0);
                pw_advance(&w, 1500);
            }
        }
        else for (unsigned i = 0; i < 3; i++)
        {
            unsigned pick = w.challenge.targets[i];
            if (k == 2) for (unsigned j = 0; j < 4; j++)
                if (w.challenge.cards[j] == w.challenge.targets[i]) pick = j;
            if (pw_request(&w, (pw_command_t)(PW_MEMORY_0 + pick)) != PW_OK) return false;
        }
        if (w.challenge.phase != PG_WON || w.memory_wins != 1 || w.settlements[PW_MEMORY] != 1 || w.mood != 63) return false;
        pw_request(&w, PW_MEMORY_0);
        if (w.settlements[PW_MEMORY] != 1) return false;
        pw_request(&w, PW_MEMORY_CANCEL);
        if (pw_request(&w, command) != PW_COOLDOWN || pw_request(&w, PW_LIFE) != PW_COOLDOWN) return false;
        pw_advance(&w, 60000);
        if (pw_request(&w, command) != PW_OK || !arrive(&w)) return false;
        pw_request(&w, PW_MEMORY_CANCEL);
        if (w.phase != PW_IDLE || w.memory_wins != 1) return false;
    }
    pet_world_t capped; room(&capped, PW_CANTEEN); capped.mood = 100;
    if (pw_request(&capped, PW_MEAL_GAME) != PW_OK || !arrive(&capped)) return false;
    for (unsigned i = 0; i < 3; i++)
        pw_request(&capped, (pw_command_t)(PW_MEMORY_0 + capped.challenge.targets[i]));
    return capped.mood == 100 && capped.mood_gain == 0 && capped.memory_wins == 1;
}
static bool life(void)
{
    for (pw_scene_t s = PW_DORM; s <= PW_GYM; s++)
    {
        if (s == PW_CAMPUS) continue;
        pet_world_t w; room(&w, s);
        if (pw_request(&w, PW_LIFE) != PW_OK || w.mood != 60 || !arrive(&w) || w.phase != PW_ACTION) return false;
        if (w.mood != 60 || pw_request(&w, PW_LIFE) != PW_BUSY) return false;
        pw_advance(&w, 4000);
        if (w.phase != PW_IDLE || w.mood != 62 || w.memory_wins || w.journal_progress) return false;
        if (pw_request(&w, PW_LIFE) != PW_COOLDOWN) return false;
        room(&w, s);
        if (pw_request(&w, PW_LIFE) != PW_OK || pw_walk_to(&w, 275, 425) != PW_OK) return false;
        pw_advance(&w, 15000);
        if (w.mood != 60 || w.settlements[PW_MEMORY]) return false;
    }
    return true;
}
static bool needs(void)
{
    pet_world_t w; room(&w, PW_DORM);
    w.food = 19; w.mood = 10; w.journal_progress = 7;
    if (strcmp(pw_need_text(&w), "肚子饿啦，去食堂吃饭吧")) return false;
    w.food = 20;
    if (strcmp(pw_need_text(&w), "想和你贴贴，也想玩一会")) return false;
    w.mood = 30;
    if (strcmp(pw_need_text(&w), "今天状态不错，随心逛逛")) return false;
    w.phase = PW_SLEEP;
    return !strcmp(pw_need_text(&w), "正在休息，点床可以叫醒");
}
bool pw_life_selftest(void (*report)(const char *, bool))
{
    bool a = games(), b = life(), c = needs();
    if (report)
    {
        report("three-games-once-shared-cooldown-cancel", a);
        report("four-life-corners-path-deferred-retarget", b);
        report("need-hints-real-state-priority", c);
    }
    return a && b && c;
}
