#include "pet_selftest.h"
#include "pet_model.h"
#include <limits.h>
#include <string.h>

static bool same_model(const pet_model_t *a, const pet_model_t *b)
{
    return a->food == b->food && a->mood == b->mood && a->sleeping == b->sleeping &&
           a->scene == b->scene && a->study_cooldown_ms == b->study_cooldown_ms &&
           a->action == b->action && a->action_ms == b->action_ms &&
           a->feed_cooldown_ms == b->feed_cooldown_ms && a->touch_cooldown_ms == b->touch_cooldown_ms &&
           a->food_elapsed_ms == b->food_elapsed_ms && a->mood_elapsed_ms == b->mood_elapsed_ms &&
           a->rest_elapsed_ms == b->rest_elapsed_ms;
}

static bool feed_boundaries(void)
{
    pet_model_t p;
    pet_model_init(&p);
    if (p.food != 70 || p.mood != 60 || pet_model_view(&p) != PET_IDLE)
        return false;
    if (pet_model_event(&p, PET_FEED) != PET_OK || p.food != 85)
        return false;
    for (unsigned i = 0; i < 20; ++i)
        if (pet_model_event(&p, PET_FEED) != PET_BUSY || p.food != 85)
            return false;
    pet_model_advance(&p, 1199);
    if (pet_model_event(&p, PET_REST) != PET_BUSY)
        return false;
    pet_model_advance(&p, 1);
    if (pet_model_event(&p, PET_FEED) != PET_COOLDOWN)
        return false;
    pet_model_advance(&p, 800);
    if (pet_model_event(&p, PET_FEED) != PET_OK || p.food != 100)
        return false;
    pet_model_advance(&p, 2000);
    return pet_model_event(&p, PET_FEED) == PET_FULL && p.feed_cooldown_ms == 0;
}

static bool touch_boundaries(void)
{
    pet_model_t p;
    pet_model_init(&p);
    p.mood = 95;
    if (pet_model_event(&p, PET_TOUCH) != PET_OK || p.mood != 100)
        return false;
    if (pet_model_view(&p) != PET_PETTING)
        return false;
    pet_model_advance(&p, 3000);
    if (pet_model_event(&p, PET_TOUCH) != PET_FULL || pet_model_view(&p) != PET_HAPPY)
        return false;
    p.mood = 60;
    if (pet_model_event(&p, PET_TOUCH) != PET_OK)
        return false;
    pet_model_advance(&p, 900);
    return pet_model_event(&p, PET_TOUCH) == PET_COOLDOWN && p.mood == 70;
}

static bool elapsed_time(void)
{
    pet_model_t one, split;
    pet_model_init(&one);
    pet_model_init(&split);
    pet_model_advance(&one, 600001);
    pet_model_advance(&split, 299999);
    if (split.food != 70)
        return false;
    pet_model_advance(&split, 1);
    if (split.food != 69)
        return false;
    pet_model_advance(&split, 300001);
    if (one.food != 68 || one.mood != 59 || !same_model(&one, &split))
        return false;
    pet_model_advance(&one, UINT32_MAX);
    return one.food == 0 && one.mood == 0 && one.food_elapsed_ms < 300000 &&
           one.mood_elapsed_ms < 600000;
}

static bool rest_and_resume(void)
{
    pet_model_t p;
    pet_model_init(&p);
    pet_model_advance(&p, 599999);
    if (pet_model_event(&p, PET_REST) != PET_OK)
        return false;
    if (pet_model_event(&p, PET_FEED) != PET_ASLEEP || pet_model_event(&p, PET_TOUCH) != PET_ASLEEP)
        return false;
    pet_model_advance(&p, 60000);
    if (p.mood != 61 || pet_model_view(&p) != PET_SLEEPING)
        return false;
    pet_model_event(&p, PET_REST);
    pet_model_advance(&p, 1);
    return p.mood == 60 && !p.sleeping && p.rest_elapsed_ms == 0;
}

static bool display_priority(void)
{
    pet_model_t p;
    pet_model_init(&p);
    p.food = 10;
    p.mood = 10;
    if (pet_model_view(&p) != PET_HUNGRY)
        return false;
    pet_model_event(&p, PET_TOUCH);
    if (pet_model_view(&p) != PET_PETTING)
        return false;
    pet_model_advance(&p, 900);
    if (pet_model_view(&p) != PET_HUNGRY)
        return false;
    p.food = 30;
    return pet_model_view(&p) == PET_LONELY;
}

static bool clock_wrap_and_fraction(void)
{
    uint32_t fraction = 0;
    if (pet_clock_elapsed(UINT32_MAX - 4U, 5U, 1000, &fraction) != 10)
        return false;
    uint32_t total = 0;
    for (uint32_t i = 0; i < 128; ++i)
        total += pet_clock_elapsed(i, i + 1U, 128, &fraction);
    return total == 1000 && fraction == 0;
}

static bool invalid_and_reset(void)
{
    pet_model_t p, before;
    pet_model_init(&p);
    before = p;
    if (pet_model_event(&p, (pet_event_t)999) != PET_INVALID || !same_model(&p, &before))
        return false;
    pet_model_event(&p, PET_REST);
    pet_model_advance(&p, UINT32_MAX);
    if (p.food != 0 || p.mood != 100)
        return false;
    pet_model_init(&p);
    return same_model(&p, &before);
}

static bool campus_travel(void)
{
    pet_model_t p, before;
    pet_model_init(&p);
    if (p.scene != PET_DORM) return false;
    before = p;
    if (pet_model_event(&p, PET_GO_CLASSROOM) != PET_OK) return false;
    before.scene = PET_CLASSROOM;
    if (!same_model(&p, &before)) return false;
    if (pet_model_event(&p, PET_GO_CANTEEN) != PET_OK) return false;
    before.scene = PET_CANTEEN;
    if (!same_model(&p, &before)) return false;
    if (pet_model_event(&p, PET_GO_DORM) != PET_OK) return false;
    before.scene = PET_DORM;
    if (!same_model(&p, &before)) return false;
    pet_model_event(&p, PET_STUDY);
    before = p;
    if (pet_model_event(&p, PET_GO_DORM) != PET_BUSY || !same_model(&p, &before)) return false;
    pet_model_advance(&p, 8000);
    pet_model_event(&p, PET_REST);
    before = p;
    if (p.scene != PET_DORM || !p.sleeping) return false;
    return pet_model_event(&p, PET_GO_CLASSROOM) == PET_ASLEEP && same_model(&p, &before) &&
           pet_model_event(&p, PET_STUDY) == PET_ASLEEP && same_model(&p, &before);
}

static bool campus_study(void)
{
    pet_model_t p, before;
    pet_model_init(&p);
    if (pet_model_event(&p, PET_STUDY) != PET_OK || p.food != 65 || p.mood != 65 ||
        p.scene != PET_CLASSROOM || p.action_ms != 5000 || p.study_cooldown_ms != 8000 ||
        pet_model_view(&p) != PET_STUDYING) return false;
    before = p;
    for (unsigned i = 0; i < 20; ++i)
        if (pet_model_event(&p, PET_STUDY) != PET_BUSY || !same_model(&p, &before)) return false;
    if (pet_model_event(&p, PET_REST) != PET_BUSY) return false;
    pet_model_advance(&p, 4999);
    if (pet_model_view(&p) != PET_STUDYING) return false;
    pet_model_advance(&p, 1);
    if (p.action_ms || pet_model_event(&p, PET_STUDY) != PET_COOLDOWN) return false;
    pet_model_advance(&p, 3000);
    if (pet_model_event(&p, PET_STUDY) != PET_OK || p.food != 60 || p.mood != 70) return false;
    pet_model_init(&p);
    p.food = 9;
    before = p;
    if (pet_model_event(&p, PET_STUDY) != PET_LOW_FOOD || !same_model(&p, &before)) return false;
    p.food = 10; p.mood = 98;
    if (pet_model_event(&p, PET_STUDY) != PET_OK || p.food != 5 || p.mood != 100) return false;
    pet_model_advance(&p, 8000);
    p.food = 70;
    before = p;
    return pet_model_event(&p, PET_STUDY) == PET_FULL && same_model(&p, &before);
}

static bool campus_routing(void)
{
    pet_model_t p;
    pet_model_init(&p);
    if (pet_model_event(&p, PET_FEED) != PET_OK || p.scene != PET_CANTEEN) return false;
    pet_model_advance(&p, 2000);
    if (pet_model_event(&p, PET_TOUCH) != PET_OK || p.scene != PET_CANTEEN) return false;
    pet_model_advance(&p, 3000);
    if (pet_model_event(&p, PET_REST) != PET_OK || p.scene != PET_DORM || !p.sleeping) return false;
    pet_model_advance(&p, 60000);
    if (p.mood != 71 || pet_model_event(&p, PET_REST) != PET_OK || p.sleeping) return false;
    if (pet_model_event(&p, PET_GO_CLASSROOM) != PET_OK || p.scene != PET_CLASSROOM) return false;
    pet_model_init(&p);
    return p.scene == PET_DORM && p.study_cooldown_ms == 0 && p.food == 70 && p.mood == 60;
}

bool pet_selftest_run(pet_test_report_fn report)
{
    static const struct { const char *name; bool (*run)(void); } cases[] = {
        {"feed-clamp-spam-cooldown", feed_boundaries},
        {"touch-clamp-cooldown", touch_boundaries},
        {"elapsed-partition-large-gap", elapsed_time},
        {"rest-reject-resume", rest_and_resume},
        {"view-priority", display_priority},
        {"clock-wrap-fraction", clock_wrap_and_fraction},
        {"invalid-event-reset", invalid_and_reset},
        {"campus-travel-guards", campus_travel},
        {"campus-study-cost-cooldown", campus_study},
        {"campus-action-routing-reset", campus_routing}
    };
    bool all = true;
    for (unsigned i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i)
    {
        bool passed = cases[i].run();
        all = all && passed;
        if (report)
            report(cases[i].name, passed);
    }
    return all;
}
