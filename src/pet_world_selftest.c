#include "pet_world.h"
#include <math.h>
#include <string.h>
#define CHECK(x)                                                                                             \
    do                                                                                                       \
    {                                                                                                        \
        if (!(x))                                                                                            \
            return false;                                                                                    \
    } while (0)
static bool arrive(pet_world_t *w)
{
    for (unsigned i = 0; i < 3000 && w->phase == PW_WALK; i++)
        pw_advance(w, 10);
    return w->phase == PW_ACTION;
}
static void travel(pet_world_t *w, pw_scene_t s)
{
    pw_request(w, (pw_command_t)(PW_GO_DORM + s));
    pw_advance(w, 20000);
}
static bool initial(void)
{
    pet_world_t w;
    pw_init(&w);
    CHECK(w.food == 70 && w.mood == 60 && w.scene == PW_DORM && w.phase == PW_IDLE);
    CHECK(pw_request(&w, PW_EAT) == PW_WRONG_ROOM);
    CHECK(w.food == 70);
    return true;
}
static bool deferred(void)
{
    pet_world_t w;
    pw_init(&w);
    CHECK(pw_request(&w, PW_CODE) == PW_OK);
    CHECK(w.phase == PW_WALK && w.food == 70 && w.settlements[PW_CODE] == 0);
    CHECK(arrive(&w));
    CHECK(w.food == 66 && w.mood == 63 && w.settlements[PW_CODE] == 1);
    return true;
}
static bool retarget(void)
{
    pet_world_t w;
    pw_init(&w);
    pw_request(&w, PW_CODE);
    pw_advance(&w, 300);
    float x = w.x, y = w.y;
    CHECK(pw_request(&w, PW_REST) == PW_OK);
    CHECK(w.x == x && w.y == y && w.food == 70);
    CHECK(arrive(&w));
    CHECK(w.action == PW_REST && w.settlements[PW_CODE] == 0);
    return true;
}
static bool arrival_recheck(void)
{
    pet_world_t w;
    pw_init(&w);
    w.food = 8;
    pw_request(&w, PW_CODE);
    w.food = 7;
    for (int i = 0; i < 1000 && w.phase == PW_WALK; i++)
        pw_advance(&w, 10);
    CHECK(w.phase == PW_IDLE && w.food == 7 && w.settlements[PW_CODE] == 0);
    return true;
}
static bool no_spam(void)
{
    pet_world_t w;
    pw_init(&w);
    pw_request(&w, PW_CODE);
    CHECK(arrive(&w));
    uint8_t f = w.food, m = w.mood;
    for (int i = 0; i < 30; i++)
        CHECK(pw_request(&w, PW_CODE) == PW_BUSY);
    CHECK(w.food == f && w.mood == m && w.settlements[PW_CODE] == 1);
    CHECK(pw_request(&w, PW_GO_CLASSROOM) == PW_BUSY);
    return true;
}
static bool cooldown(void)
{
    pet_world_t w;
    pw_init(&w);
    pw_request(&w, PW_CODE);
    CHECK(arrive(&w));
    pw_advance(&w, (uint32_t)(w.action_end - w.now));
    CHECK(w.phase == PW_IDLE && w.cooldown[PW_CODE] == w.now + 10000);
    CHECK(pw_request(&w, PW_CODE) == PW_COOLDOWN);
    pw_advance(&w, 10000);
    CHECK(pw_request(&w, PW_CODE) == PW_OK);
    return true;
}
static bool capped_meal(void)
{
    pet_world_t w;
    pw_init(&w);
    travel(&w, PW_CANTEEN);
    CHECK(w.scene == PW_CANTEEN && w.food == 70 && w.mood == 60);
    w.food = 96;
    pw_request(&w, PW_EAT);
    CHECK(arrive(&w));
    CHECK(w.food == 100 && w.gain == 4);
    pw_advance(&w, 4000);
    CHECK(pw_request(&w, PW_EAT) == PW_COOLDOWN);
    pw_advance(&w, 2000);
    CHECK(pw_request(&w, PW_EAT) == PW_FULL);
    return true;
}
static bool high_mood_study(void)
{
    pet_world_t w;
    pw_init(&w);
    travel(&w, PW_CLASSROOM);
    w.mood = 100;
    w.food = 10;
    CHECK(pw_request(&w, PW_STUDY) == PW_OK);
    CHECK(arrive(&w));
    CHECK(w.food == 3 && w.mood == 100);
    CHECK(pw_duration(PW_STUDY) == 8000);
    return true;
}
static bool sleep_wake(void)
{
    pet_world_t w;
    pw_init(&w);
    pw_request(&w, PW_REST);
    CHECK(arrive(&w));
    pw_advance(&w, 600);
    CHECK(w.phase == PW_SLEEP);
    pw_visual_t v;
    pw_visual(&w, &v);
    CHECK(v.frame == 18 && fabsf(v.x - 94) < .01f && fabsf(v.y - 154) < .01f);
    CHECK(pw_request(&w, PW_GO_CANTEEN) == PW_ASLEEP);
    pw_advance(&w, 60000);
    CHECK(w.mood == 63);
    CHECK(pw_request(&w, PW_REST) == PW_OK);
    pw_advance(&w, 500);
    pw_point_t ladder = pw_node(PW_DORM, 6);
    CHECK(w.phase == PW_IDLE && w.x == ladder.x && w.y == ladder.y);
    return true;
}
static bool touch_cancel(void)
{
    pet_world_t w;
    pw_init(&w);
    pw_request(&w, PW_CODE);
    pw_advance(&w, 100);
    float x = w.x, y = w.y;
    CHECK(pw_request(&w, PW_TOUCH) == PW_OK);
    CHECK(w.x == x && w.y == y && w.mood == 63 && w.settlements[PW_CODE] == 0);
    pw_advance(&w, 1500);
    CHECK(w.phase == PW_IDLE && w.cooldown[PW_TOUCH] == w.now + PW_TOUCH_COOLDOWN_MS);
    return true;
}
static bool elapsed_partition(void)
{
    pet_world_t a, b;
    pw_init(&a);
    pw_init(&b);
    a.phase = b.phase = PW_SLEEP;
    pw_advance(&a, 3600000);
    for (int i = 0; i < 3600; i++)
        pw_advance(&b, 1000);
    CHECK(a.food == b.food && a.mood == b.mood && a.rest_elapsed == b.rest_elapsed &&
          a.food_elapsed == b.food_elapsed);
    pw_init(&a);
    pw_advance(&a, 86400000);
    CHECK(a.food == 0 && a.mood == 0);
    return true;
}
static bool jump_support(void)
{
    pet_world_t w;
    pw_init(&w);
    pw_request(&w, PW_CODE);
    CHECK(arrive(&w));
    pw_advance(&w, 250);
    pw_visual_t v;
    pw_visual(&w, &v);
    CHECK(v.airborne && v.frame < 16 && v.lift > 16);
    pw_advance(&w, 300);
    pw_visual(&w, &v);
    CHECK(v.frame >= 16 && v.frame <= 17 && v.occlusion && fabsf(v.y - 263) < .01f);
    pw_advance(&w, (uint32_t)(w.action_end - w.now - 250));
    pw_visual(&w, &v);
    CHECK(v.airborne && v.frame < 16);
    pw_advance(&w, 250);
    pw_point_t desk = pw_node(PW_DORM, 7);
    CHECK(w.phase == PW_IDLE && w.x == desk.x && w.y == desk.y);
    return true;
}
static bool all_directions(void)
{
    pet_world_t w;
    pw_init(&w);
    static const pw_point_t steps[] = {
        {0, 21}, {-21, 21}, {-21, 0}, {-21, -21},
        {0, -21}, {21, -21}, {21, 0}, {21, 21}};
    for (unsigned i = 0; i < 8; i++)
    {
        w.x = 325;
        w.y = 365;
        CHECK(pw_walk_to(&w, w.x + steps[i].x, w.y + steps[i].y) == PW_OK);
        pw_advance(&w, 50);
        CHECK(w.phase == PW_WALK && w.facing == i);
    }
    return true;
}
static bool walking_pose(void)
{
    pet_world_t w;
    pw_visual_t pose;
    pw_init(&w);
    w.x = 325;
    w.y = 365;
    pw_advance(&w, 250);
    CHECK(pw_walk_to(&w, 355, 365) == PW_OK);
    pw_visual(&w, &pose);
    CHECK(pose.facing == 6 && pose.frame == 12 && pose.lift == 0);
    pw_advance(&w, 175);
    pw_visual(&w, &pose);
    CHECK(pose.frame == 13 && pose.lift > 0);
    CHECK(pw_walk_to(&w, w.x, 405) == PW_OK);
    pw_visual(&w, &pose);
    CHECK(pose.facing == 0 && pose.frame == 1);
    pw_advance(&w, 1000);
    pw_visual(&w, &pose);
    CHECK(w.phase == PW_IDLE && pose.facing == 0 && pose.frame == 0 && pose.lift == 0);
    return true;
}
static bool indoor_floor(void)
{
    pet_world_t w;
    pw_init(&w);
    CHECK(pw_walk_to(&w, 185, 306) == PW_OK);
    pw_advance(&w, 1500);
    CHECK(w.phase == PW_IDLE && w.x == 185 && w.y == 306);
    CHECK(pw_walk_to(&w, 302, 306) == PW_OK);
    pw_advance(&w, 100);
    float x = w.x, y = w.y;
    CHECK(pw_walk_to(&w, 231, 305) == PW_INVALID);
    CHECK(pw_walk_to(&w, 120, 90) == PW_INVALID);
    CHECK(w.x == x && w.y == y && w.point_goal.x == 302);
    CHECK(pw_walk_to(&w, 296, 309) == PW_OK && w.x == x && w.y == y);
    for (int i = 0; i < 260; i++)
    {
        pw_advance(&w, 10);
        CHECK(pw_room_ground(PW_DORM, w.x, w.y));
    }
    CHECK(w.phase == PW_IDLE && w.x == 296 && w.y == 309);
    pw_advance(&w, 10000);
    CHECK(w.x == 296 && w.y == 309);
    return true;
}
static bool other_room_detours(void)
{
    for (pw_scene_t scene = PW_CLASSROOM; scene <= PW_CANTEEN; scene++)
    {
        pet_world_t w;
        pw_init(&w);
        w.scene = scene;
        w.x = 28;
        w.y = 274;
        CHECK(pw_walk_to(&w, 173, 274) == PW_OK);
        for (int i = 0; i < 400 && w.phase == PW_WALK; i++)
        {
            pw_advance(&w, 10);
            CHECK(pw_room_ground(scene, w.x, w.y));
        }
        CHECK(w.phase == PW_IDLE && w.x == 173 && w.y == 274);
    }
    return true;
}
static bool reset_invalid(void)
{
    pet_world_t w;
    pw_init(&w);
    CHECK(pw_request(&w, (pw_command_t)99) == PW_INVALID);
    w.food = 0;
    w.mood = 100;
    pw_init(&w);
    CHECK(w.food == 70 && w.mood == 60 && w.phase == PW_IDLE);
    return true;
}
static void campus_spawn(pet_world_t *w)
{
    pw_init(w);
    w->scene = PW_CAMPUS;
    w->node = 0;
    pw_point_t origin = pw_node(PW_CAMPUS, 0);
    w->x = origin.x;
    w->y = origin.y;
}
static bool on_road(const pet_world_t *w)
{
    for (unsigned i = 0; i < pw_road_count(); i++)
    {
        pw_point_t a, b;
        pw_road(i, &a, &b);
        float dx = b.x - a.x, dy = b.y - a.y;
        float t = fmaxf(0, fminf(1, ((w->x - a.x) * dx + (w->y - a.y) * dy) / (dx * dx + dy * dy)));
        if (hypotf(w->x - a.x - t * dx, w->y - a.y - t * dy) < .1f)
            return true;
    }
    return false;
}
static bool campus_point_routes(void)
{
    pet_world_t w;
    campus_spawn(&w);
    CHECK(pw_walk_to(&w, 800, 850) == PW_OK);
    pw_advance(&w, 1000);
    float x = w.x, y = w.y;
    CHECK(pw_walk_to(&w, 570, 690) == PW_OK && w.x == x && w.y == y);
    for (int i = 0; i < 300; i++)
    {
        pw_advance(&w, 50);
        CHECK(on_road(&w));
    }
    CHECK(fabsf(w.x - 570) < .1f && fabsf(w.y - 690) < .1f && w.phase == PW_IDLE);
    CHECK(pw_walk_to(&w, 670, 690) == PW_OK);
    pw_advance(&w, 3000);
    CHECK(fabsf(w.x - 670) < .1f);
    CHECK(pw_walk_to(&w, 730, 690) == PW_OK);
    pw_advance(&w, 900);
    CHECK(fabsf(w.x - 730) < .1f && w.phase == PW_IDLE);
    const pw_point_t targets[] = {{385, 618}, {1020, 720}, {800, 885}, {705, 1060}};
    for (unsigned k = 0; k < 4; k++)
    {
        CHECK(pw_walk_to(&w, targets[k].x, targets[k].y) == PW_OK);
        for (int n = 0; n < 500; n++)
        {
            pw_advance(&w, 50);
            CHECK(on_road(&w));
        }
    }
    CHECK(pw_walk_to(&w, 278, 518) == PW_INVALID);
    CHECK(pw_walk_to(&w, -1, 390) == PW_INVALID);
    CHECK(pw_walk_to(&w, NAN, 390) == PW_INVALID);
    CHECK(pw_walk_to(&w, INFINITY, 390) == PW_INVALID);
    return true;
}
static bool campus_doors(void)
{
    for (int room = 0; room < 3; room++)
    {
        pet_world_t w;
        pw_init(&w);
        w.scene = (pw_scene_t)room;
        w.node = 3;
        pw_point_t p = pw_node(w.scene, 3);
        w.x = p.x;
        w.y = p.y;
        CHECK(pw_request(&w, PW_EXIT) == PW_OK && w.scene == room);
        for (int i = 0; i < 1000 && w.scene != PW_CAMPUS; i++)
            pw_advance(&w, 20);
        pw_advance(&w, 500);
        CHECK(w.scene == PW_CAMPUS && w.phase == PW_IDLE);
        p = pw_node(PW_CAMPUS, 13 + room);
        CHECK(fabsf(w.x - p.x) < .1f && fabsf(w.y - p.y) < .1f);
        pw_advance(&w, 120000);
        CHECK(w.x == p.x && w.y == p.y);
        CHECK(pw_request(&w, (pw_command_t)(PW_ENTER_DORM + room)) == PW_OK);
        pw_advance(&w, 10000);
        CHECK(w.scene == room);
    }
    return true;
}
static bool library_round_trip(void)
{
    pet_world_t w;
    campus_spawn(&w);
    CHECK(pw_request(&w, PW_ENTER_LIBRARY) == PW_OK);
    pw_advance(&w, 10000);
    CHECK(pw_in_library(&w) && w.phase == PW_IDLE);
    CHECK(pw_walk_to(&w, 47, 294) == PW_INVALID);
    CHECK(pw_walk_to(&w, 24, 332) == PW_INVALID);
    CHECK(pw_walk_to(&w, 180, 302) == PW_INVALID);
    CHECK(pw_walk_to(&w, 75, 402) == PW_OK);
    for (int i = 0; i < 500 && w.phase == PW_WALK; i++)
    {
        CHECK(!(w.x >= 88 && w.x <= 275 && w.y >= 280 && w.y <= 356));
        pw_advance(&w, 20);
    }
    CHECK(w.phase == PW_IDLE && w.x == 75 && w.y == 402);
    CHECK(pw_request(&w, PW_STUDY) == PW_OK);
    pw_advance(&w, 12000);
    CHECK(pw_in_library(&w) && w.phase == PW_IDLE && w.settlements[PW_STUDY] == 1);
    CHECK(pw_request(&w, PW_EXIT) == PW_OK);
    CHECK(pw_in_library(&w));
    for (int i = 0; i < 500 && w.scene == PW_CLASSROOM; i++)
    {
        CHECK(pw_in_library(&w));
        pw_advance(&w, 20);
    }
    pw_advance(&w, 500);
    pw_point_t door = pw_node(PW_CAMPUS, PW_LIBRARY_DOOR);
    CHECK(w.scene == PW_CAMPUS && w.phase == PW_IDLE && w.building_id == 0);
    CHECK(w.x == door.x && w.y == door.y);
    return true;
}
static bool teaching_room_round_trips(void)
{
    static const struct
    {
        pw_command_t command;
        unsigned building, door;
        float left, top, right, bottom;
        pw_point_t goal;
    } cases[] = {
        {PW_ENTER_PUBLIC, PW_PUBLIC_BUILDING, 54, 12, 259, 244, 310, {92, 317}},
        {PW_ENTER_LAB, PW_LAB_BUILDING, 16, 216, 268, 315, 327, {183, 369}},
        {PW_ENTER_WISDOM, PW_SMART_BUILDING, 24, 198, 245, 292, 321, {174, 372}}
    };
    for (unsigned i = 0; i < sizeof(cases) / sizeof(cases[0]); i++)
    {
        pet_world_t w;
        campus_spawn(&w);
        CHECK(pw_request(&w, cases[i].command) == PW_OK);
        pw_advance(&w, 20000);
        CHECK(w.scene == PW_CLASSROOM && w.phase == PW_IDLE &&
              w.building_id == cases[i].building && w.return_node == cases[i].door);
        CHECK(pw_walk_to(&w, (cases[i].left + cases[i].right) / 2,
                         (cases[i].top + cases[i].bottom) / 2) == PW_INVALID);
        CHECK(pw_walk_to(&w, cases[i].goal.x, cases[i].goal.y) == PW_OK);
        for (unsigned step = 0; step < 500 && w.phase == PW_WALK; step++)
        {
            CHECK(w.x < cases[i].left || w.x > cases[i].right ||
                  w.y < cases[i].top || w.y > cases[i].bottom);
            pw_advance(&w, 20);
        }
        CHECK(w.phase == PW_IDLE && w.x == cases[i].goal.x && w.y == cases[i].goal.y);
        CHECK(pw_request(&w, PW_STUDY) == PW_OK);
        pw_advance(&w, 15000);
        CHECK(w.phase == PW_IDLE && w.settlements[PW_STUDY] == 1);
        CHECK(pw_request(&w, PW_EXIT) == PW_OK && w.building_id == cases[i].building);
        pw_advance(&w, 15000);
        CHECK(w.scene == PW_CAMPUS && w.phase == PW_IDLE && w.node == cases[i].door);
    }
    return true;
}
static bool campus_guards(void)
{
    pet_world_t w;
    pw_init(&w);
    w.phase = PW_SLEEP;
    CHECK(pw_request(&w, PW_EXIT) == PW_ASLEEP && w.destination < 0);
    w.phase = PW_ACTION;
    CHECK(pw_request(&w, PW_EXIT) == PW_BUSY);
    campus_spawn(&w);
    CHECK(pw_request(&w, PW_EAT) == PW_WRONG_ROOM);
    CHECK(pw_request(&w, PW_EXIT) == PW_WRONG_ROOM);
    CHECK(pw_request(&w, PW_TOUCH) == PW_OK);
    CHECK(pw_walk_to(&w, 800, 850) == PW_BUSY);
    CHECK(pw_request(&w, PW_ENTER_DORM) == PW_BUSY);
    return true;
}
static bool campus_clock_camera(void)
{
    pet_world_t a, b;
    campus_spawn(&a);
    b = a;
    CHECK(pw_walk_to(&a, 625, 1085) == PW_OK && pw_walk_to(&b, 625, 1085) == PW_OK);
    pw_advance(&a, 20000);
    for (int i = 0; i < 1000; i++)
        pw_advance(&b, 20);
    CHECK(fabsf(a.x - b.x) < .1f && fabsf(a.y - b.y) < .1f && a.phase == b.phase && a.food == b.food);
    float x, y;
    a.x = 0;
    a.y = 0;
    pw_camera(&a, &x, &y);
    CHECK(x == 0 && y == 0);
    a.y = 120;
    pw_camera(&a, &x, &y);
    CHECK(y == 0);
    CHECK(pw_walk_to(&a, 570, -100) == PW_INVALID);
    a.x = PW_CAMPUS_WIDTH;
    a.y = PW_CAMPUS_HEIGHT;
    pw_camera(&a, &x, &y);
    CHECK(x == PW_CAMPUS_WIDTH - 390 && y == PW_CAMPUS_HEIGHT - 450);
    a.x = 800;
    a.y = 690;
    pw_camera(&a, &x, &y);
    CHECK(x == 605 && y == 460);
    a.x += 1;
    pw_camera(&a, &x, &y);
    CHECK(x == 606);
    a.scene = PW_DORM;
    pw_camera(&a, &x, &y);
    CHECK(x == 0 && y == 0);
    return true;
}
static bool campus_all_roads_connected(void)
{
    /* Walk from the safe junction to every real node. */
    for (unsigned i = 0; i < PW_PATH_CAPACITY; i++)
    {
        pw_point_t goal = pw_node(PW_CAMPUS, i);
        if (goal.x == 0 && goal.y == 0)
            continue;
        pet_world_t w;
        campus_spawn(&w);
        CHECK(pw_walk_to(&w, goal.x, goal.y) == PW_OK);
        for (unsigned n = 0; n < 110 && w.phase == PW_WALK; n++)
        {
            pw_advance(&w, 350);
            CHECK(on_road(&w));
        }
        CHECK(w.phase == PW_IDLE && fabsf(w.x-goal.x) < .1f && fabsf(w.y-goal.y) < .1f);
    }
    return true;
}

static bool rebalanced_actions(void)
{
    pet_world_t w;
    pw_init(&w);
    travel(&w, PW_CANTEEN);
    CHECK(pw_request(&w, PW_EAT) == PW_OK && arrive(&w));
    CHECK(w.food == 90 && w.gain == 20 && w.settlements[PW_EAT] == 1);
    pw_advance(&w, pw_duration(PW_EAT));
    travel(&w, PW_CLASSROOM);
    CHECK(pw_request(&w, PW_STUDY) == PW_OK && arrive(&w));
    CHECK(w.food == 83 && w.mood == 64 && w.settlements[PW_STUDY] == 1);
    pw_advance(&w, pw_duration(PW_STUDY));
    travel(&w, PW_DORM);
    w.mood = 98;
    CHECK(pw_request(&w, PW_REST) == PW_OK && arrive(&w));
    pw_advance(&w, (uint32_t)(w.action_end - w.now));
    pw_advance(&w, 59999);
    CHECK(w.mood == 98);
    pw_advance(&w, 1);
    CHECK(w.mood == 100);
    return true;
}

static bool gym_cycle(void)
{
    pet_world_t w;
    campus_spawn(&w);
    CHECK(pw_request(&w, PW_ENTER_GYM) == PW_OK);
    pw_advance(&w, 8000);
    CHECK(w.scene == PW_GYM && w.phase == PW_IDLE);
    w.food = 9;
    CHECK(pw_request(&w, PW_WORKOUT) == PW_LOW_FOOD);
    w.food = 70;
    CHECK(pw_request(&w, PW_WORKOUT) == PW_OK && arrive(&w));
    CHECK(w.food == 70 && w.mood == 60 && w.settlements[PW_WORKOUT] == 0);
    CHECK(pw_request(&w, PW_WORKOUT) == PW_BUSY);
    pw_advance(&w, (uint32_t)(w.action_end - w.now));
    CHECK(w.food == 64 && w.mood == 66 && w.gain == -6 && w.mood_gain == 6);
    CHECK(w.settlements[PW_WORKOUT] == 1 && w.phase == PW_IDLE);
    CHECK(pw_request(&w, PW_WORKOUT) == PW_COOLDOWN);
    CHECK(pw_request(&w, PW_EXIT) == PW_OK);
    pw_advance(&w, 9000);
    pw_point_t entrance = pw_node(PW_CAMPUS, pw_campus_door(PW_GYM));
    CHECK(w.scene == PW_CAMPUS && w.x == entrance.x && w.y == entrance.y);
    return true;
}

static bool takeout_cycle(void)
{
    pet_world_t w;
    pw_init(&w);
    CHECK(pw_request(&w, PW_TAKEOUT) == PW_NO_ORDER);
    CHECK(pw_request(&w, PW_ORDER) == PW_OK);
    CHECK(pw_request(&w, PW_ORDER) == PW_ORDER_ACTIVE);
    travel(&w, PW_CLASSROOM);
    pw_advance(&w, (uint32_t)(w.order_due - w.now - 1));
    CHECK(w.order == PW_ORDER_DELIVERING);
    pw_advance(&w, 1);
    CHECK(w.order == PW_ORDER_READY && w.scene == PW_CLASSROOM);
    travel(&w, PW_DORM);
    w.food = 100;
    CHECK(pw_request(&w, PW_TAKEOUT) == PW_FULL && w.order == PW_ORDER_READY);
    w.food = 95;
    CHECK(pw_request(&w, PW_TAKEOUT) == PW_OK && arrive(&w));
    CHECK(w.food == 95 && w.order == PW_ORDER_READY && w.settlements[PW_TAKEOUT] == 0);
    CHECK(pw_request(&w, PW_TAKEOUT) == PW_BUSY);
    pw_advance(&w, (uint32_t)(w.action_end - w.now));
    CHECK(w.food == 100 && w.gain == 5 && w.order == PW_ORDER_NONE);
    CHECK(w.settlements[PW_TAKEOUT] == 1 && pw_request(&w, PW_ORDER) == PW_COOLDOWN);
    pw_advance(&w, PW_TAKEOUT_COOLDOWN_MS);
    CHECK(pw_request(&w, PW_ORDER) == PW_FULL); w.food = 90;
    CHECK(pw_request(&w, PW_ORDER) == PW_OK);
    return true;
}

static bool expanded_room_bounds(void)
{
    static const pw_scene_t rooms[] = {PW_DORM, PW_CLASSROOM, PW_CANTEEN, PW_GYM};
    static const pw_point_t blocked[] = {{40,400},{340,372},{340,360},{320,360}};
    for (unsigned i = 0; i < sizeof(rooms) / sizeof(rooms[0]); i++)
    {
        pet_world_t w;
        pw_init(&w);
        w.scene = rooms[i];
        pw_point_t start = pw_node(w.scene, 3);
        w.x = start.x;
        w.y = start.y;
        CHECK(pw_walk_to(&w, blocked[i].x, blocked[i].y) == PW_INVALID);
        CHECK(pw_walk_to(&w, 370, 435) == PW_OK);
        for (unsigned n = 0; n < 200 && w.phase == PW_WALK; n++)
        {
            pw_advance(&w, 30);
            CHECK(pw_room_ground(w.scene, w.x, w.y));
        }
        CHECK(w.phase == PW_IDLE && w.x == 370 && w.y == 435);
    }
    return true;
}

static bool album_unlock(void)
{
    pet_world_t w;
    pw_init(&w);
    w.now = 10000;
    w.wallet.coins = PE_MAX_COINS;
    pw_point_t canteen = pw_node(PW_CANTEEN, 3);
    for (unsigned i = 0; i < PE_MENU_COUNT; i++)
    {
        w.scene = PW_CANTEEN;
        w.phase = PW_SEATED;
        w.action_start = w.now - 1000;
        w.food = 10;
        w.cooldown[PW_EAT] = 0;
        w.x = canteen.x; w.y = canteen.y;
        CHECK(pw_request(&w, (pw_command_t)(PW_MEAL_FIRST + i)) == PW_OK);
        CHECK((w.dishes & (1U << i)) == 0);
        pw_advance(&w, (uint32_t)(w.action_end - w.now) + 10);
        CHECK(w.dishes == ((1U << (i + 1)) - 1U) && pw_dish_count(&w) == i + 1);
    }
    /* A second helping unlocks nothing new and does not corrupt the mask. */
    w.phase = PW_SEATED;
    w.action_start = w.now - 1000;
    w.food = 10;
    w.cooldown[PW_EAT] = 0;
    CHECK(pw_request(&w, (pw_command_t)(PW_MEAL_FIRST + 2)) == PW_OK);
    pw_advance(&w, (uint32_t)(w.action_end - w.now) + 10);
    CHECK(w.dishes == 0x003F && pw_dish_count(&w) == PE_MENU_COUNT);
    /* Delivery uses bits 6..11 and unlocks only after pickup. */
    pw_point_t dorm = pw_node(PW_DORM, 3);
    for (unsigned i = 0; i < PE_MENU_COUNT; i++)
    {
        w.scene = PW_DORM;
        w.phase = PW_IDLE;
        w.order = PW_ORDER_READY;
        w.order_choice = (uint8_t)(i + 1);
        w.food = 10;
        w.cooldown[PW_TAKEOUT] = 0;
        w.x = dorm.x; w.y = dorm.y;
        CHECK(pw_request(&w, PW_TAKEOUT) == PW_OK);
        for (unsigned n = 0; n < 500 && w.phase == PW_WALK; n++) pw_advance(&w, 20);
        pw_advance(&w, (uint32_t)(w.action_end - w.now) + 10);
        CHECK(w.dishes == (0x003FU | (((1U << (i + 1)) - 1U) << PE_MENU_COUNT)));
        CHECK(pw_dish_count(&w) == PE_MENU_COUNT + i + 1);
    }
    CHECK(w.dishes == 0x0FFFU && pw_dish_count(&w) == 12);
    return true;
}

static bool wrong_room_feedback(void)
{
    pet_world_t w;
    pw_init(&w);
    CHECK(pw_request(&w, PW_ENTER_CLASSROOM) == PW_WRONG_ROOM);
    CHECK(w.scene == PW_DORM && w.phase == PW_IDLE && w.message != 0 && *pw_message_text(&w));
    w.scene = PW_CAMPUS;
    w.message = 0;
    CHECK(pw_request(&w, PW_EXIT) == PW_WRONG_ROOM);
    CHECK(w.scene == PW_CAMPUS && w.message != 0 && *pw_message_text(&w));
    return true;
}

static bool retired_journal(void)
{
    pet_world_t w; pw_init(&w); w.journal_progress = 7;
    CHECK(pw_request(&w, PW_JOURNAL_CLAIM) == PW_INVALID);
    CHECK(w.wallet.coins == PE_START_COINS && !w.wallet.owned && !w.journal_stamps);
    return true;
}

static bool memory_rounds_and_guards(void)
{
    pet_world_t w;
    pw_init(&w);
    CHECK(pw_request(&w, PW_MEMORY_START) == PW_WRONG_ROOM);
    travel(&w, PW_CLASSROOM);
    CHECK(pw_request(&w, PW_MEMORY_START) == PW_OK);
    for (unsigned i = 0; i < 1000 && w.phase == PW_WALK; i++) pw_advance(&w, 10);
    CHECK(w.phase == PW_MINIGAME && w.memory.phase == PM_SHOW);
    CHECK(pw_request(&w, PW_MEMORY_0) == PW_OK && !w.memory.input_at && !w.memory_wins);
    pw_advance(&w, pm_remaining(&w.memory));
    unsigned wrong = (w.memory.sequence[0] + 1) % 4;
    CHECK(pw_request(&w, (pw_command_t)(PW_MEMORY_0 + wrong)) == PW_OK);
    CHECK(w.memory.phase == PM_LOST && !w.memory_wins && w.mood == 60);
    pw_advance(&w, 1500);
    CHECK(w.phase == PW_IDLE && pw_request(&w, PW_MEMORY_START) == PW_OK);
    w.mood = 99;
    for (unsigned i = 0; i < 1000 && w.phase == PW_WALK; i++) pw_advance(&w, 10);
    for (unsigned round = 0; round < 3; round++)
    {
        CHECK(w.memory.length == 4 + round);
        CHECK(pm_remaining(&w.memory) == (4 + round) * (650 - 50 * round));
        pw_advance(&w, pm_remaining(&w.memory));
        CHECK(w.memory.phase == PM_INPUT);
        unsigned length = w.memory.length;
        for (unsigned i = 0; i < length; i++)
        {
            unsigned symbol = w.memory.sequence[i];
            CHECK(pw_request(&w, (pw_command_t)(PW_MEMORY_0 + symbol)) == PW_OK);
        }
    }
    CHECK(w.memory.phase == PM_WON && w.memory_wins == 1 && w.mood == 100 && w.mood_gain == 1 && w.journal_progress == 1);
    CHECK(pw_request(&w, PW_MEMORY_0) == PW_OK && w.memory_wins == 1 && w.mood == 100);
    pw_advance(&w, 1500);
    CHECK(pw_request(&w, PW_MEMORY_START) == PW_COOLDOWN);
    pw_advance(&w, PW_MEMORY_COOLDOWN_MS);
    CHECK(pw_request(&w, PW_MEMORY_START) == PW_OK);
    for (unsigned i = 0; i < 1000 && w.phase == PW_WALK; i++) pw_advance(&w, 10);
    CHECK(pw_request(&w, PW_MEMORY_CANCEL) == PW_OK && w.phase == PW_IDLE && w.memory_wins == 1);
    CHECK(pw_request(&w, PW_MEMORY_START) == PW_OK);
    for (unsigned i = 0; i < 1000 && w.phase == PW_WALK; i++) pw_advance(&w, 10);
    CHECK(w.memory.phase == PM_SHOW);
    pw_advance(&w, pm_remaining(&w.memory));
    unsigned coins = w.wallet.coins;
    pw_advance(&w, PM_INPUT_MS - 1);
    CHECK(w.memory.phase == PM_INPUT && w.wallet.coins == coins);
    pw_advance(&w, 1);
    CHECK(w.memory.phase == PM_LOST && w.wallet.coins == coins + 2 && w.memory_wins == 1);
    pw_advance(&w, 1500);
    CHECK(w.phase == PW_IDLE && w.wallet.coins == coins + 2);
    return true;
}

bool pw_campus_selftest(void (*report)(const char *, bool))
{
    static const struct
    {
        const char *name;
        bool (*run)(void);
    } cases[] = {{"campus-point-retarget-road", campus_point_routes},
                 {"campus-three-building-doors", campus_doors},
                 {"library-stays-visible-until-campus", library_round_trip},
                 {"teaching-room-detours-and-original-doors", teaching_room_round_trips},
                 {"campus-guards-no-wander", campus_guards},
                 {"campus-clock-camera", campus_clock_camera},
                 {"campus-all-road-endpoints", campus_all_roads_connected}};
    bool all = true;
    for (unsigned i = 0; i < sizeof(cases) / sizeof(cases[0]); i++)
    {
        bool ok = cases[i].run();
        if (report)
            report(cases[i].name, ok);
        all = all && ok;
    }
    return all;
}
static bool volume_controls(void)
{
    pet_world_t w;
    pw_init(&w);
    CHECK(w.volume == 5);
    for (unsigned i=0; i<20; i++) CHECK(pw_request(&w, PW_VOLUME_DOWN) == PW_OK);
    CHECK(w.volume == 0 && w.food == 70 && w.mood == 60);
    for (unsigned i=0; i<20; i++) CHECK(pw_request(&w, PW_VOLUME_UP) == PW_OK);
    CHECK(w.volume == 10 && w.food == 70 && w.mood == 60);
    CHECK(pw_request(&w, PW_MUSIC_TOGGLE) == PW_OK && w.sound_mode == 1);
    CHECK(pw_request(&w, PW_MUSIC_TOGGLE) == PW_OK && w.sound_mode == 2);
    w.phase = PW_SLEEP;
    CHECK(pw_request(&w, PW_VOLUME_DOWN) == PW_OK && w.phase == PW_SLEEP && w.volume == 9);
    w.sound_mode = 0; w.volume = 0;
    CHECK(pw_request(&w, PW_VOLUME_UP) == PW_OK && w.volume == 1 && w.sound_mode == 1);
    return true;
}
static bool activity_and_workstudy(void)
{
    pet_world_t w; pw_init(&w);
    w.scene = PW_CAMPUS; w.phase = PW_IDLE; w.food = 50; w.mood = 50;
    CHECK(pw_request(&w, PW_DANCE) == PW_OK);
    CHECK(w.mood == 55 && w.phase == PW_IDLE);
    CHECK(pw_request(&w, PW_BADMINTON) == PW_COOLDOWN);
    w.now += PW_ACTIVITY_COOLDOWN_MS;
    CHECK(pw_request(&w, PW_BADMINTON) == PW_OK && w.mood == 59);
    w.scene = PW_CLASSROOM; w.now += 1;
    unsigned coins = w.wallet.coins;
    CHECK(pw_request(&w, PW_WORKSTUDY) == PW_OK);
    CHECK(w.wallet.coins == coins + PW_WORK_COINS);
    CHECK(pw_request(&w, PW_WORKSTUDY) == PW_COOLDOWN);
    w.now += PW_WORK_COOLDOWN_MS;
    w.scene = PW_CAMPUS;
    CHECK(pw_request(&w, PW_WORKSTUDY) == PW_WRONG_ROOM);
    w.scene = PW_CLASSROOM; w.mood = 100;
    CHECK(pw_request(&w, PW_DANCE) == PW_WRONG_ROOM);
    return true;
}
static bool real_time_routine(void)
{
    pet_world_t w;pw_init(&w);uint32_t utc;
    CHECK(pt_parse((const uint8_t *)"20261108230000",14,&utc));
    w.clock=(pt_clock_t){utc,true,true,PT_SYNCED,PT_OK,false};
    CHECK(strstr(pw_state_text(&w),"困")!=NULL);
    w.scene=PW_CLASSROOM;w.node=6;pw_point_t p=pw_node(PW_CLASSROOM,6);w.x=p.x;w.y=p.y;
    w.food=w.mood=50;CHECK(pw_request(&w,PW_STUDY)==PW_OK);
    for(unsigned i=0;i<100 && w.phase!=PW_ACTION;i++)pw_advance(&w,40);
    CHECK(w.activity_kind==1 && w.action_tired && w.food==46 && w.mood==51);
    uint64_t end=w.action_end,now=w.now;w.clock.utc-=14*3600U;
    CHECK(w.now==now && w.action_end==end);pw_advance(&w,12000);
    CHECK(w.settlements[PW_STUDY]==1 && w.mood==51);
    return true;
}
bool pw_selftest(void (*report)(const char *, bool))
{
    static const struct
    {
        const char *name;
        bool (*run)(void);
    } cases[] = {{"real-clock-routine-and-no-repeat", real_time_routine},
                 {"volume-bounds-and-music-toggle", volume_controls},
                 {"initial-scene", initial},
                 {"arrive-once", deferred},
                 {"retarget-no-snap", retarget},
                 {"arrival-eligibility", arrival_recheck},
                 {"busy-no-queue", no_spam},
                 {"cooldown-from-finish", cooldown},
                 {"capped-meal", capped_meal},
                 {"full-mood-study", high_mood_study},
                 {"sleep-wake-support", sleep_wake},
                 {"touch-cancels-intent", touch_cancel},
                 {"elapsed-and-long-gap", elapsed_partition},
                 {"jump-and-seat", jump_support},
                 {"eight-directions", all_directions},
                 {"walking-pose-follows-distance-and-route", walking_pose},
                 {"indoor-floor-detour-and-retarget", indoor_floor},
                 {"classroom-and-canteen-detours", other_room_detours},
                 {"reset-invalid", reset_invalid},
                 {"retired-journal-cannot-claim", retired_journal},
                 {"memory-rounds-wrong-cancel-and-once", memory_rounds_and_guards},
                 {"rebalanced-actions-and-sleep-clamp", rebalanced_actions},
                 {"gym-access-workout-and-return", gym_cycle},
                 {"takeout-delivery-guards-and-once", takeout_cycle},
                 {"album-unlock-once-and-takeout", album_unlock},
                 {"wrong-room-feedback", wrong_room_feedback},
                 {"activity-and-workstudy", activity_and_workstudy},
                 {"four-room-decor-and-expanded-floor", expanded_room_bounds}};
    bool all = true;
    for (unsigned i = 0; i < sizeof(cases) / sizeof(cases[0]); i++)
    {
        bool ok = cases[i].run();
        if (report)
            report(cases[i].name, ok);
        all = all && ok;
    }
    bool campus = pw_campus_selftest(report);
    bool life = pw_life_selftest(report);
    bool economy = pw_economy_selftest(report);
    return all && campus && life && economy;
}
