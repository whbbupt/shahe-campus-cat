/* Shared deterministic NOR simulator. No production driver is reachable here. */
#include "pet_save.h"
#include <string.h>
static struct
{
    uint8_t bytes[PS_AREA_SIZE];
    int budget, erases, writes;
    bool fail_read, fail_erase;
} ram;
static bool rd(void *ctx, uint32_t o, uint8_t *p, size_t n)
{
    (void)ctx;
    if (ram.fail_read || o > PS_AREA_SIZE || n > PS_AREA_SIZE - o) return false;
    memcpy(p, ram.bytes + o, n); return true;
}
static bool wr(void *ctx, uint32_t o, const uint8_t *p, size_t n)
{
    (void)ctx;
    if (o > PS_AREA_SIZE || n > PS_AREA_SIZE - o) return false;
    ram.writes++;
    for (size_t i = 0; i < n; i++)
    {
        if (ram.budget == 0 || (ram.bytes[o + i] & p[i]) != p[i]) return false;
        ram.bytes[o + i] &= p[i];
        if (ram.budget > 0) ram.budget--;
    }
    return true;
}
static bool er(void *ctx, uint32_t o, size_t n)
{
    (void)ctx;
    if (o % PS_SECTOR_SIZE || n != PS_SECTOR_SIZE || o + n > PS_AREA_SIZE) return false;
    ram.erases++;
    /* A failed erase can partially change the target; newest sector stays intact. */
    memset(ram.bytes + o, 255, ram.fail_erase ? n / 2 : n);
    return !ram.fail_erase;
}
static ps_io_t io(void)
{
    return (ps_io_t){NULL, rd, wr, er};
}
static void blank(void)
{
    memset(&ram, 0, sizeof(ram)); memset(ram.bytes, 255, sizeof(ram.bytes)); ram.budget = -1;
}
#define CHECK(x) do { if (!(x)) return false; } while (0)
static bool empty_restore(void)
{
    blank(); pet_save_t s; pet_world_t w; pw_init(&w);
    CHECK(ps_open(&s, io(), &w) == PS_EMPTY && w.food == 70);
    w.food = 63; w.mood = 82; w.scene = PW_CLASSROOM;
    w.clock.trusted = w.clock.valid = true; w.clock.utc = 1704038400U;
    w.food_elapsed = 12345; w.mood_elapsed = 23456; w.cooldown[PW_STUDY] = 7000;
    CHECK(ps_save(&s, &w) == PS_OK);
    CHECK(ram.bytes[0] == 'C' && ram.bytes[1] == 'A' && ram.bytes[2] == 'T' && ram.bytes[3] == '1');
    CHECK(ram.bytes[12] == 63 && ram.bytes[14] == PW_CLASSROOM);
    pet_world_t restored; CHECK(ps_open(&s, io(), &restored) == PS_OK);
    CHECK(restored.food == 63 && restored.mood == 82 && restored.scene == PW_CLASSROOM);
    CHECK(restored.clock.trusted && !restored.clock.valid && !restored.clock.utc);
    CHECK(restored.now == 0 && restored.phase == PW_IDLE && restored.action == PW_NONE && restored.target == PW_NONE);
    CHECK(restored.node == 3 && restored.food_elapsed == 12345 && restored.mood_elapsed == 23456);
    CHECK(restored.cooldown[PW_STUDY] == 7000 && restored.settlements[PW_STUDY] == 0);
    return true;
}
static bool sleep_restore(void)
{
    blank(); pet_save_t s; pet_world_t w; pw_init(&w); ps_open(&s, io(), &w);
    w.phase = PW_SLEEP; w.rest_elapsed = 59999;
    CHECK(ps_save(&s, &w) == PS_OK && ps_open(&s, io(), &w) == PS_OK);
    CHECK(w.phase == PW_SLEEP && w.node == 6 && w.rest_elapsed == 59999);
    pw_advance(&w, 1); CHECK(w.mood == 63); /* elapsed time survives, offline time does not */
    CHECK(pw_request(&w, PW_REST) == PW_OK); pw_advance(&w, 500);
    CHECK(w.phase == PW_IDLE); return true;
}
static bool partial_writes(void)
{
    for (int cut = 0; cut <= 64; cut++)
    {
        blank(); pet_save_t s; pet_world_t w; pw_init(&w); ps_open(&s, io(), &w);
        CHECK(ps_save(&s, &w) == PS_OK); w.food = 52; ram.budget = cut;
        ps_result_t result = ps_save(&s, &w); ram.budget = -1;
        pet_world_t r; CHECK(ps_open(&s, io(), &r) == PS_OK);
        CHECK(r.food == (cut == 64 ? 52 : 70));
        CHECK((cut == 64 && result == PS_OK) || (cut < 64 && result == PS_IO_ERROR));
    }
    return true;
}
static bool bad_record(void)
{
    blank(); pet_save_t s; pet_world_t w; pw_init(&w); ps_open(&s, io(), &w);
    CHECK(ps_save(&s, &w) == PS_OK); w.food = 42; CHECK(ps_save(&s, &w) == PS_OK);
    ram.bytes[PS_SECTOR_SIZE + 12] ^= 1;
    CHECK(ps_open(&s, io(), &w) == PS_OK && w.food == 70);
    ram.bytes[12] ^= 1;
    CHECK(ps_open(&s, io(), &w) == PS_EMPTY); return true;
}
static bool rotate_and_erase(void)
{
    blank(); pet_save_t s; pet_world_t w; pw_init(&w); ps_open(&s, io(), &w);
    for (unsigned i = 0; i < 128; i++) { w.food = (uint8_t)(i % 100); CHECK(ps_save(&s, &w) == PS_OK); }
    CHECK(ram.erases == 0 && s.sequence == 128);
    w.food = 91; ram.fail_erase = true; CHECK(ps_save(&s, &w) == PS_IO_ERROR);
    CHECK(ps_open(&s, io(), &w) == PS_OK && w.food == 27);
    ram.fail_erase = false; w.food = 91;
    CHECK(ps_save(&s, &w) == PS_OK); CHECK(ps_open(&s, io(), &w) == PS_OK && w.food == 91);
    CHECK(ram.erases == 1); /* retry used the erased half rather than erasing newest */
    return true;
}
static bool io_failures(void)
{
    blank(); pet_save_t s; pet_world_t w; pw_init(&w); ps_open(&s, io(), &w);
    CHECK(ps_save(&s, &w) == PS_OK); ram.fail_read = true;
    w.food = 60; CHECK(ps_save(&s, &w) == PS_IO_ERROR && s.sequence == 1);
    CHECK(ps_open(&s, io(), &w) == PS_IO_ERROR && !s.enabled);
    ram.fail_read = false; CHECK(ps_open(&s, io(), &w) == PS_OK && w.food == 70);
    return true;
}
static bool no_busy_replay(void)
{
    blank(); pet_save_t s; pet_world_t w; pw_init(&w); ps_open(&s, io(), &w);
    CHECK(ps_save(&s, &w) == PS_OK);
    CHECK(pw_request(&w, PW_TOUCH) == PW_OK && ps_save(&s, &w) == PS_WAIT);
    CHECK(ps_open(&s, io(), &w) == PS_OK && w.mood == 60 && w.phase == PW_IDLE);
    w.phase = PW_TRANSITION; CHECK(ps_save(&s, &w) == PS_WAIT);
    w.phase = PW_WALK; w.purpose = 1; CHECK(ps_save(&s, &w) == PS_WAIT);
    w.purpose = 0; w.food = 101; CHECK(ps_save(&s, &w) == PS_WAIT);
    return true;
}
static bool scheduler(void)
{
    blank(); pet_save_t s; pet_world_t w; pw_init(&w); ps_open(&s, io(), &w);
    CHECK(ps_poll(&s, &w) == PS_WAIT); w.now = 2999; CHECK(ps_poll(&s, &w) == PS_WAIT);
    w.now = 3000; CHECK(ps_poll(&s, &w) == PS_OK);
    w.mood = 80; w.now = 4000; CHECK(ps_poll(&s, &w) == PS_WAIT);
    w.now = 7000; CHECK(ps_poll(&s, &w) == PS_WAIT);
    w.now = 13000; CHECK(ps_poll(&s, &w) == PS_OK);
    int writes = ram.writes; w.now = 73000; CHECK(ps_poll(&s, &w) == PS_UNCHANGED && ram.writes == writes);
    w.food_elapsed = 60000; w.now = 133000; CHECK(ps_poll(&s, &w) == PS_OK);
    w.mood = 81; w.now = 134000; CHECK(ps_poll(&s, &w) == PS_WAIT);
    w.now = 143000; ram.budget = 1; CHECK(ps_poll(&s, &w) == PS_IO_ERROR);
    ram.budget = -1; w.now = 152999; CHECK(ps_poll(&s, &w) == PS_WAIT);
    w.now = 153000; CHECK(ps_poll(&s, &w) == PS_OK);
    CHECK(ps_open(&s, io(), &w) == PS_OK && w.mood == 81);
    return true;
}
static bool visible_status(void)
{
    blank(); pet_save_t s; pet_world_t w; pw_init(&w); ps_open(&s, io(), &w);
    CHECK(!ps_visible_saved(&s, &w)); CHECK(ps_save(&s, &w) == PS_OK);
    CHECK(ps_visible_saved(&s, &w)); int writes = ram.writes;
    w.food--; CHECK(!ps_visible_saved(&s, &w)); w.food++;
    w.mood++; CHECK(!ps_visible_saved(&s, &w)); w.mood--;
    w.scene = PW_CLASSROOM; CHECK(!ps_visible_saved(&s, &w)); w.scene = PW_DORM;
    w.phase = PW_ACTION; CHECK(!ps_visible_saved(&s, &w));
    w.phase = PW_TRANSITION; CHECK(!ps_visible_saved(&s, &w));
    w.phase = PW_WALK; w.purpose = 1; CHECK(!ps_visible_saved(&s, &w));
    w.purpose = 0; CHECK(ps_visible_saved(&s, &w));
    w.phase = PW_SLEEP; CHECK(!ps_visible_saved(&s, &w));
    CHECK(ps_save(&s, &w) == PS_OK && ps_visible_saved(&s, &w));
    w.phase = PW_IDLE; CHECK(!ps_visible_saved(&s, &w));
    CHECK(ram.writes == writes + 2); /* queries themselves perform no writes */
    return true;
}
static bool campus_restore(void)
{
    blank();
    pet_save_t s;
    pet_world_t w;
    pw_init(&w);
    ps_open(&s, io(), &w);
    w.scene = PW_CAMPUS;
    w.node = 0;
    w.x = 560;
    w.y = 560;
    CHECK(ps_save(&s, &w) == PS_OK && s.saved[4] == 7);
    w.x = 960;
    w.y = 900;
    CHECK(ps_open(&s, io(), &w) == PS_OK && w.scene == PW_CAMPUS);
    CHECK(w.x == pw_node(PW_CAMPUS, 0).x && w.y == pw_node(PW_CAMPUS, 0).y && w.phase == PW_IDLE && w.target == PW_NONE);
    CHECK(pw_walk_to(&w, 800, 810) == PW_OK && !ps_visible_saved(&s, &w));
    CHECK(ps_save(&s, &w) == PS_WAIT);
    pw_advance(&w, 3000);
    CHECK(ps_visible_saved(&s, &w));
    return true;
}
static bool takeout_resume(void)
{
    blank();
    pet_save_t s;
    pet_world_t w;
    pw_init(&w);
    CHECK(ps_open(&s, io(), &w) == PS_EMPTY);
    CHECK(ps_save(&s, &w) == PS_OK);
    CHECK(pw_request(&w, PW_ORDER) == PW_OK && !ps_visible_saved(&s, &w));
    pw_advance(&w, 8000);
    CHECK(ps_save(&s, &w) == PS_OK);
    CHECK(ps_open(&s, io(), &w) == PS_OK && w.order == PW_ORDER_DELIVERING && w.order_due == 22000);
    pw_advance(&w, 21999);
    CHECK(w.order == PW_ORDER_DELIVERING);
    pw_advance(&w, 1);
    CHECK(w.order == PW_ORDER_READY && !ps_visible_saved(&s, &w));
    CHECK(ps_save(&s, &w) == PS_OK && ps_visible_saved(&s, &w));
    CHECK(ps_open(&s, io(), &w) == PS_OK && w.order == PW_ORDER_READY);
    CHECK(pw_request(&w, PW_TAKEOUT) == PW_OK);
    CHECK(ps_save(&s, &w) == PS_WAIT);
    pw_advance(&w, 10000);
    CHECK(w.order == PW_ORDER_NONE && w.food == 82);
    ram.budget = 63;
    CHECK(ps_save(&s, &w) == PS_IO_ERROR);
    ram.budget = -1;
    CHECK(ps_open(&s, io(), &w) == PS_OK && w.order == PW_ORDER_READY && w.food == 70);
    CHECK(pw_request(&w, PW_TAKEOUT) == PW_OK);
    pw_advance(&w, 10000);
    CHECK(ps_save(&s, &w) == PS_OK);
    CHECK(ps_open(&s, io(), &w) == PS_OK && w.order == PW_ORDER_NONE && w.food == 82);
    CHECK(w.cooldown[PW_TAKEOUT] > w.now && pw_request(&w, PW_TAKEOUT) == PW_NO_ORDER);
    w.scene = PW_GYM;
    w.cooldown[PW_WORKOUT] = 14000;
    CHECK(ps_save(&s, &w) == PS_OK);
    CHECK(ps_open(&s, io(), &w) == PS_OK && w.scene == PW_GYM && w.cooldown[PW_WORKOUT] == 14000);
    CHECK(w.x == pw_node(PW_GYM, 3).x && w.y == pw_node(PW_GYM, 3).y);
    return true;
}

static bool takeout_autosave(void)
{
    blank();
    pet_save_t s;
    pet_world_t w;
    pw_init(&w);
    CHECK(ps_open(&s, io(), &w) == PS_EMPTY && ps_save(&s, &w) == PS_OK);
    CHECK(pw_request(&w, PW_ORDER) == PW_OK);
    for (unsigned i = 0; i < 3; i++)
    {
        CHECK(ps_poll(&s, &w) == PS_WAIT);
        pw_advance(&w, 1000);
    }
    CHECK(ps_poll(&s, &w) == PS_OK && ps_visible_saved(&s, &w));
    CHECK(ps_open(&s, io(), &w) == PS_OK && w.order_due == 27000);
    pw_advance(&w, 27000);
    CHECK(ps_poll(&s, &w) == PS_WAIT);
    pw_advance(&w, 3000);
    CHECK(ps_poll(&s, &w) == PS_OK);
    CHECK(ps_open(&s, io(), &w) == PS_OK && w.order == PW_ORDER_READY);
    return true;
}
static bool sound_restore(void)
{
    blank();
    pet_save_t s;
    pet_world_t w;
    pw_init(&w);
    CHECK(ps_open(&s, io(), &w) == PS_EMPTY && ps_save(&s, &w) == PS_OK);
    for (unsigned mode = 0; mode < 3; mode++) {
        while (w.sound_mode != mode) CHECK(pw_request(&w, PW_SOUND_NEXT) == PW_OK);
        CHECK(ps_save(&s, &w) == PS_OK);
        CHECK(ps_open(&s, io(), &w) == PS_OK && w.sound_mode == mode && w.phase == PW_IDLE);
    }
    CHECK(pw_request(&w, PW_SOUND_NEXT) == PW_OK && !ps_visible_saved(&s, &w));
    CHECK(ps_poll(&s, &w) == PS_WAIT);
    pw_advance(&w, 10000);
    CHECK(ps_poll(&s, &w) == PS_OK);
    CHECK(ps_open(&s, io(), &w) == PS_OK && w.sound_mode == 1);
    for (unsigned level=0; level<=10; level++) {
        w.volume=(uint8_t)level;
        CHECK(ps_save(&s, &w) == PS_OK);
        CHECK(ps_open(&s, io(), &w) == PS_OK && w.volume == level && ps_visible_saved(&s, &w));
    }
    CHECK(pw_request(&w, PW_VOLUME_DOWN) == PW_OK && !ps_visible_saved(&s, &w));
    CHECK(ps_poll(&s, &w) == PS_WAIT);
    pw_advance(&w, 10000);
    CHECK(ps_poll(&s, &w) == PS_OK);
    CHECK(ps_open(&s, io(), &w) == PS_OK && w.volume == 9);
    w.volume=11;
    CHECK(ps_save(&s, &w) == PS_INVALID);
    return true;
}
static bool journal_restore(void)
{
    blank();
    pet_save_t s;
    pet_world_t w;
    pw_init(&w);
    CHECK(ps_open(&s, io(), &w) == PS_EMPTY && ps_save(&s, &w) == PS_OK);
    w.journal_progress = 7;
    CHECK(!ps_visible_saved(&s, &w) && ps_save(&s, &w) == PS_OK);
    CHECK(pw_request(&w, PW_JOURNAL_CLAIM) == PW_OK);
    CHECK(!ps_visible_saved(&s, &w));
    ram.budget = 63;
    CHECK(ps_save(&s, &w) == PS_IO_ERROR);
    ram.budget = -1;
    CHECK(ps_open(&s, io(), &w) == PS_OK && w.journal_page == 1 &&
          w.journal_progress == 7 && w.journal_stamps == 0);
    CHECK(pw_request(&w, PW_JOURNAL_CLAIM) == PW_OK && ps_save(&s, &w) == PS_OK);
    CHECK(ps_open(&s, io(), &w) == PS_OK && w.journal_page == 2 && w.journal_stamps == 1);
    CHECK(w.journal_badges == 1 && w.journal_progress == 0 && ps_visible_saved(&s, &w));
    w.memory_wins = 17;
    w.cooldown[PW_MEMORY] = PW_MEMORY_COOLDOWN_MS;
    w.scene = PW_CLASSROOM;
    CHECK(ps_save(&s, &w) == PS_OK);
    w.phase = PW_MINIGAME;
    w.action = PW_MEMORY;
    pm_start(&w.memory, 4);
    CHECK(ps_save(&s, &w) == PS_WAIT);
    CHECK(ps_open(&s, io(), &w) == PS_OK && w.memory_wins == 17 && w.memory.phase == PM_OFF);
    CHECK(w.phase == PW_IDLE && w.cooldown[PW_MEMORY] == PW_MEMORY_COOLDOWN_MS);
    return true;
}
bool ps_selftest(void (*report)(const char *, bool))
{
    static const struct { const char *name; bool (*run)(void);
    } cases[] = {
        {"format-restore-cooldowns", empty_restore},        {"sleep-elapsed-wake", sleep_restore},
        {"all-65-write-cut-points", partial_writes},        {"crc-fallback", bad_record},
        {"rotation-partial-erase", rotate_and_erase},       {"short-io-no-success", io_failures},
        {"no-unfinished-action-replay", no_busy_replay},    {"autosave-throttle-checkpoint-retry", scheduler},
        {"visible-state-save-consistency", visible_status}, {"campus-safe-restore", campus_restore},
        {"takeout-resume-and-atomic-pickup", takeout_resume},
        {"takeout-autosave-phase-key", takeout_autosave},
        {"journal-atomic-stamp-and-game-restore", journal_restore},
        {"sound-mode-autosave-and-reboot", sound_restore}};
    bool all = true;
    for (unsigned i = 0; i < sizeof(cases) / sizeof(cases[0]); i++)
    {
        bool ok = cases[i].run(); if (report) report(cases[i].name, ok); all = all && ok;
    }
    memset(&ram, 0, sizeof(ram)); return all;
}
