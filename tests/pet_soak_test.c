/* Host-only 7-day soak: random legal intents, bounds, and reboot snapshots.
 * RAM NOR simulation only; this is not physical flash evidence. */
#include "pet_save.h"
#include "pet_world.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static uint8_t flash[PS_AREA_SIZE];
static unsigned writes, erases;

static bool rd(void *c, uint32_t o, uint8_t *p, size_t n)
{
    (void)c;
    if (o + n > PS_AREA_SIZE) return false;
    memcpy(p, flash + o, n);
    return true;
}
static bool wr(void *c, uint32_t o, const uint8_t *p, size_t n)
{
    (void)c;
    if (o + n > PS_AREA_SIZE) return false;
    writes++;
    for (size_t i = 0; i < n; i++)
    {
        if ((flash[o + i] & p[i]) != p[i]) return false;
        flash[o + i] &= p[i];
    }
    return true;
}
static bool er(void *c, uint32_t o, size_t n)
{
    (void)c;
    if (o % PS_SECTOR_SIZE || n != PS_SECTOR_SIZE || o + n > PS_AREA_SIZE) return false;
    erases++;
    memset(flash + o, 255, n);
    return true;
}

static uint32_t rng = 0x5EED1234U;
static uint32_t next(void)
{
    rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5;
    return rng;
}

static void check_world(const pet_world_t *w)
{
    assert(w->food <= 100 && w->mood <= 100);
    assert(w->scene <= PW_GYM && w->phase <= PW_SEATED);
    assert(w->wallet.coins <= PE_MAX_COINS);
    assert(w->wallet.owned <= PE_FURNITURE_MASK);
    assert((w->wallet.equipped & ~w->wallet.owned) == 0);
    assert(w->dishes <= 0x0FFFU);
    assert(w->order <= PW_ORDER_READY && w->order_choice <= PE_MENU_COUNT);
    assert(w->order != PW_ORDER_NONE || w->order_choice == 0);
    assert(w->checkin_day == 0 || (w->checkin_day >= PW_CHECKIN_MIN_DAY && w->checkin_day <= PW_CHECKIN_MAX_DAY));
    assert(w->meal_choice < PE_MENU_COUNT);
}

static const pw_command_t intents[] = {
    PW_EAT, PW_TOUCH, PW_REST, PW_STUDY, PW_CODE, PW_WAKE, PW_WORKOUT, PW_TAKEOUT,
    PW_ORDER, PW_MEMORY_START, PW_MEMORY_CANCEL, PW_GO_DORM, PW_GO_CLASSROOM,
    PW_GO_CANTEEN, PW_EXIT, PW_ENTER_DORM, PW_ENTER_CLASSROOM, PW_ENTER_CANTEEN,
    PW_ENTER_GYM, PW_SIT_MEAL, PW_MEAL_LEAVE, PW_DAILY_CHECKIN, PW_BUY_PLANT,
    PW_BUY_PICTURE, PW_BUY_SHELF, PW_LIFE, PW_MEAL_GAME, PW_RHYTHM_GAME, PW_FIND_GAME};

int main(void)
{
    memset(flash, 255, sizeof(flash));
    pet_save_t s;
    pet_world_t w, restored;
    ps_io_t io = {NULL, rd, wr, er};
    pw_init(&w);
    assert(ps_open(&s, io, &w) == PS_EMPTY);
    unsigned commands = 0, saved = 0, restores = 0;
    const uint64_t sim_end = 7ULL * 24 * 3600;
    for (uint64_t sec = 0; sec < sim_end; sec += 5)
    {
        w.clock.valid = w.clock.trusted = true;
        w.clock.utc = 1704038400U + (uint32_t)sec; /* 2024-01-01 UTC, +08:00 */
        pw_advance(&w, 5000);
        if ((next() % 200) == 0)
        {
            pw_command_t cmd = intents[next() % (sizeof(intents) / sizeof(intents[0]))];
            if (cmd >= PW_MEAL_FIRST && cmd <= PW_MEAL_LAST) cmd = PW_SIT_MEAL;
            (void)pw_request(&w, cmd);
            commands++;
        }
        check_world(&w);
        if (sec != 0 && sec % 1800 == 0)
        {
            ps_result_t r = ps_save(&s, &w);
            if (r == PS_OK || r == PS_UNCHANGED)
            {
                saved++;
                assert(ps_open(&s, io, &restored) == PS_OK);
                assert(restored.food == w.food && restored.mood == w.mood);
                assert(restored.scene == w.scene && restored.wallet.coins == w.wallet.coins);
                assert(restored.wallet.owned == w.wallet.owned && restored.wallet.equipped == w.wallet.equipped);
                assert(restored.dishes == w.dishes && restored.checkin_day == w.checkin_day);
                assert(ps_visible_saved(&s, &restored));
                check_world(&restored);
                restores++;
            }
        }
    }
    assert(w.clock.valid);
    printf("[host:soak] 7 simulated days, %u intents, %u saves, %u restores PASS\n", commands, saved, restores);
    printf("[host:soak] RAM NOR writes=%u erases=%u; physical flash/long-run memory NOT tested\n", writes, erases);
    return 0;
}
