/* Real production LVGL and UI, simulated pointer and clock; not physical evidence. */
#include "lvgl.h"
#include "pet_ui.h"
#include "pet_world_assets.h"
#include "pet_shahe_assets.h"
#include "pet_world_font.h"
#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#undef assert
#define assert(c)                                                                                            \
    do                                                                                                       \
    {                                                                                                        \
        if (!(c))                                                                                            \
        {                                                                                                    \
            fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #c);                                             \
            exit(1);                                                                                         \
        }                                                                                                    \
    } while (0)
static pet_world_t world;
static pw_result_t result;
static unsigned char frame[450][390][3], draw_buffer[390 * 50 * 3];
static lv_point_t pointer;
static lv_indev_state_t pointer_state;
static void flush(lv_display_t *d, const lv_area_t *a, uint8_t *data)
{
    int width = a->x2 - a->x1 + 1;
    for (int y = a->y1; y <= a->y2; y++)
        for (int x = a->x1; x <= a->x2; x++)
        {
            assert(x >= 0 && x < 390 && y >= 0 && y < 450);
            unsigned i = ((y - a->y1) * width + x - a->x1) * 3;
            frame[y][x][0] = data[i + 2];
            frame[y][x][1] = data[i + 1];
            frame[y][x][2] = data[i];
        }
    lv_display_flush_ready(d);
}
static void read_pointer(lv_indev_t *i, lv_indev_data_t *d)
{
    (void)i;
    d->point = pointer;
    d->state = pointer_state;
}
static pw_result_t command(pw_command_t c)
{
    result = pw_request(&world, c);
    pet_ui_update(&world);
    return result;
}
static void step(uint32_t ms)
{
    pw_advance(&world, ms);
    pet_ui_update(&world);
    lv_tick_inc(ms);
    lv_timer_handler();
}
static void tap(int x, int y)
{
    pointer.x = x;
    pointer.y = y;
    pointer_state = LV_INDEV_STATE_PRESSED;
    step(40);
    step(40);
    pointer_state = LV_INDEV_STATE_RELEASED;
    step(40);
    step(40);
}
static bool shown(lv_obj_t *o);
static lv_obj_t *label_with(lv_obj_t *p, const char *name)
{
    for (unsigned i = 0; i < lv_obj_get_child_count(p); i++)
    {
        lv_obj_t *o = lv_obj_get_child(p, i);
        if (lv_obj_check_type(o, &lv_label_class) && shown(o) && !strcmp(lv_label_get_text(o), name))
            return o;
        lv_obj_t *f = label_with(o, name);
        if (f)
            return f;
    }
    return NULL;
}
static lv_obj_t *control(const char *name)
{
    lv_obj_t *l = label_with(lv_screen_active(), name);
    assert(l);
    lv_obj_t *p = lv_obj_get_parent(l);
    while (p && !lv_obj_check_type(p, &lv_button_class))
        p = lv_obj_get_parent(p);
    assert(p);
    return p;
}
static void press(const char *name)
{
    lv_obj_t *b = control(name);
    lv_obj_update_layout(lv_screen_active());
    lv_area_t a;
    lv_obj_get_coords(b, &a);
    tap((a.x1 + a.x2) / 2, (a.y1 + a.y2) / 2);
}
static unsigned timers(void)
{
    unsigned n = 0;
    for (lv_timer_t *t = lv_timer_get_next(NULL); t; t = lv_timer_get_next(t))
        n++;
    return n;
}
static bool shown(lv_obj_t *o)
{
    while (o)
    {
        if (lv_obj_has_flag(o, LV_OBJ_FLAG_HIDDEN))
            return false;
        o = lv_obj_get_parent(o);
    }
    return true;
}
static void controls_fit(lv_obj_t *p, bool hidden)
{
    hidden = hidden || lv_obj_has_flag(p, LV_OBJ_FLAG_HIDDEN);
    if (!hidden && lv_obj_check_type(p, &lv_button_class))
    {
        lv_area_t a;
        lv_obj_get_coords(p, &a);
        bool map_child = false;
        for (lv_obj_t *q = p; q; q = lv_obj_get_parent(q))
            if (lv_obj_get_width(q) == PW_CAMPUS_WIDTH)
                map_child = true;
        if (!map_child)
            assert(a.x1 >= 0 && a.y1 >= 0 && a.x2 < 390 && a.y2 < 450);
        assert(lv_obj_get_width(p) >= 44 && lv_obj_get_height(p) >= 44);
    }
    for (unsigned i = 0; i < lv_obj_get_child_count(p); i++)
        controls_fit(lv_obj_get_child(p, i), hidden);
}
static void glyphs(lv_obj_t *p)
{
    if (lv_obj_check_type(p, &lv_label_class))
    {
        const unsigned char *s = (const unsigned char *)lv_label_get_text(p);
        const lv_font_t *f = lv_obj_get_style_text_font(p, 0);
        while (*s)
        {
            uint32_t c = *s++;
            if (c >= 0xF0)
            {
                c = (c & 7) << 18;
                c |= (*s++ & 63) << 12;
                c |= (*s++ & 63) << 6;
                c |= *s++ & 63;
            }
            else if (c >= 0xE0)
            {
                c = (c & 15) << 12;
                c |= (*s++ & 63) << 6;
                c |= *s++ & 63;
            }
            else if (c >= 0xC0)
            {
                c = (c & 31) << 6;
                c |= *s++ & 63;
            }
            if (c > 32)
            {
                lv_font_glyph_dsc_t d;
                assert(lv_font_get_glyph_dsc(f, &d, c, 0));
            }
        }
    }
    for (unsigned i = 0; i < lv_obj_get_child_count(p); i++)
        glyphs(lv_obj_get_child(p, i));
}
static void screenshot(const char *name)
{
    lv_obj_update_layout(lv_screen_active());
    controls_fit(lv_screen_active(), false);
    glyphs(lv_screen_active());
    lv_refr_now(NULL);
    FILE *f = fopen(name, "wb");
    assert(f);
    fprintf(f, "P6\n390 450\n255\n");
    assert(fwrite(frame, sizeof(frame), 1, f) == 1);
    fclose(f);
    fprintf(stderr, "[host:world-ui] %s\n", name);
}
static pw_result_t point_command(float x, float y)
{
    result = pw_walk_to(&world, x, y);
    pet_ui_update(&world);
    return result;
}
static lv_obj_t *image_with(lv_obj_t *parent, const lv_image_dsc_t *source)
{
    for (unsigned i = 0; i < lv_obj_get_child_count(parent); i++)
    {
        lv_obj_t *child = lv_obj_get_child(parent, i);
        if (lv_obj_check_type(child, &lv_image_class) && lv_image_get_src(child) == source)
            return child;
        lv_obj_t *found = image_with(child, source);
        if (found)
            return found;
    }
    return NULL;
}
static void reset(void)
{
    pw_init(&world);
    pet_ui_create(command);
    pet_ui_set_point_handler(point_command);
    pet_ui_update(&world);
    lv_obj_update_layout(lv_screen_active());
}
static void idle(pw_scene_t scene)
{
    for (unsigned i = 0; i < 1000 && (world.scene != scene || world.phase != PW_IDLE); i++)
        step(40);
    assert(world.scene == scene && world.phase == PW_IDLE);
}
static void until_action(pw_command_t cmd)
{
    for (unsigned i = 0; i < 500 && world.phase == PW_WALK; i++)
        step(40);
    assert(world.phase == PW_ACTION && world.action == cmd);
}
static void settled_at(int x, int y)
{
    assert(world.phase == PW_IDLE && fabsf(world.x - x) < .1f && fabsf(world.y - y) < .1f);
}
static void check_new_floor(int block_x, int block_y)
{
    pw_scene_t scene = world.scene;
    float x = world.x, y = world.y;
    tap(block_x, block_y);
    assert(result == PW_INVALID && world.x == x && world.y == y);
    tap(372, 433);
    assert(result == PW_OK && world.phase == PW_WALK);
    idle(scene);
    settled_at(372, 433);
}
static void dorm(void)
{
    assert(world.scene == PW_DORM);
    assert(!label_with(lv_screen_active(), "路标"));
    screenshot("shahe-dorm.ppm");
    check_new_floor(40, 400);
    press("状态");
    assert(label_with(lv_screen_active(), "小猫状态"));
    pet_ui_set_save_status(PET_SAVE_SAVED);
    assert(label_with(lv_screen_active(), "已保存，重启可恢复"));
    screenshot("shahe-status.ppm");
    tap(234, 370);
    assert(world.phase == PW_IDLE); /* scrim dismisses without walking */
    tap(235, 375);
    assert(result == PW_OK && world.phase == PW_WALK);
    idle(PW_DORM);
    settled_at(235, 375);
    tap(188, 300);
    idle(PW_DORM);
    settled_at(188, 300);
    tap(301, 302);
    assert(world.phase == PW_WALK);
    for (unsigned i = 0; i < 100 && world.phase == PW_WALK; i++)
    {
        step(25);
        assert(pw_room_ground(PW_DORM, world.x, world.y));
    }
    settled_at(301, 302);
    tap(89, 207);
    assert(world.phase == PW_WALK && world.settlements[PW_CODE] == 0);
    until_action(PW_CODE);
    step(550);
    assert(world.settlements[PW_CODE] == 1);
    lv_obj_t *seat = image_with(lv_screen_active(), &shahe_seat_0);
    assert(seat && shown(seat));
    screenshot("shahe-dorm-code.ppm");
    step(pw_duration(PW_CODE));
    tap(99, 129);
    until_action(PW_REST);
    step(300);
    screenshot("shahe-dorm-climb.ppm");
    step(350);
    assert(world.phase == PW_SLEEP);
    screenshot("shahe-dorm-sleep.ppm");
    tap(100, 126);
    step(510);
    pw_point_t ladder = pw_node(PW_DORM, 6);
    settled_at((int)ladder.x, (int)ladder.y);
}
static void road(int x, int y)
{
    float cx, cy;
    pw_camera(&world, &cx, &cy);
    int sx = x - (int)lroundf(cx), sy = y - (int)lroundf(cy);
    assert(sx >= 0 && sx < 390 && sy >= 0 && sy < 450);
    tap(sx, sy);
    assert(result == PW_OK && world.phase == PW_WALK && world.purpose == 3);
    idle(PW_CAMPUS);
    settled_at(x, y);
}
static void gate(pw_scene_t scene)
{
    assert(world.phase == PW_IDLE && world.scene == PW_CAMPUS);
    pw_point_t door = pw_node(PW_CAMPUS, pw_campus_door(scene));
    settled_at((int)door.x, (int)door.y);
}
static void leave(pw_scene_t scene)
{
    press("出门");
    assert(result == PW_OK && world.scene == scene && world.purpose == 2);
    idle(PW_CAMPUS);
    gate(scene);
}
static void enter(const char *name, pw_scene_t scene)
{
    press(name);
    assert(result == PW_OK && world.destination == scene);
    idle(scene);
    float x, y;
    pw_camera(&world, &x, &y);
    assert(x == 0 && y == 0);
}
static void check_campus_tiles(void)
{
    float cx, cy;
    pw_camera(&world, &cx, &cy);
    lv_obj_update_layout(lv_screen_active());
    for (int y = 0; y < PW_CAMPUS_HEIGHT / 260; y++)
        for (int x = 0; x < PW_CAMPUS_WIDTH / 280; x++)
        {
            lv_obj_t *tile = image_with(lv_screen_active(), shahe_ground[y * (PW_CAMPUS_WIDTH / 280) + x]);
            assert(tile && !lv_obj_has_flag(tile, LV_OBJ_FLAG_CLICKABLE));
            lv_area_t bounds;
            lv_obj_get_coords(tile, &bounds);
            assert(bounds.x1 == x * 280 - (int)lroundf(cx));
            assert(bounds.y1 == y * 260 - (int)lroundf(cy));
            assert(bounds.x2 - bounds.x1 + 1 == 280 && bounds.y2 - bounds.y1 + 1 == 260);
        }
}
static void memory_demo(void)
{
    static const char *symbols[] = {"花", "叶", "月", "星"};
    press("记忆题");
    for (unsigned i = 0; i < 1000 && world.phase == PW_WALK; i++) step(20);
    assert(world.phase == PW_MINIGAME && world.memory.phase == PM_SHOW);
    screenshot("shahe-memory-show.ppm");
    press("花");
    assert(world.memory.phase == PM_SHOW && world.memory.input_at == 0);
    step(pm_remaining(&world.memory));
    unsigned wrong = (world.memory.sequence[0] + 1) % 4;
    press(symbols[wrong]);
    assert(world.memory.phase == PM_LOST && world.memory_wins == 0);
    screenshot("shahe-memory-retry.ppm");
    step(1600);
    assert(world.phase == PW_IDLE);
    press("记忆题");
    for (unsigned i = 0; i < 1000 && world.phase == PW_WALK; i++) step(20);
    for (unsigned round = 0; round < 3; round++)
    {
        step(pm_remaining(&world.memory));
        assert(world.memory.phase == PM_INPUT);
        screenshot("shahe-memory-input.ppm");
        unsigned length = world.memory.length;
        for (unsigned i = 0; i < length; i++)
            press(symbols[world.memory.sequence[i]]);
    }
    assert(world.memory.phase == PM_WON && world.memory_wins == 1);
    screenshot("shahe-memory-won.ppm");
    step(1600);
    assert(world.phase == PW_IDLE);
}
static void journal_demo(void)
{
    assert(world.phase == PW_SLEEP && world.journal_progress == 7);
    press("手账");
    assert(label_with(lv_screen_active(), "点床叫醒小猫后收贴纸"));
    screenshot("shahe-journal-complete.ppm");
    press("×");
    tap(100,126);
    step(600);
    assert(world.phase == PW_IDLE);
    press("手账");
    press("收下贴纸，翻到下一页");
    assert(world.journal_page == 2 && world.journal_stamps == 1 && world.journal_badges == 1);
    assert(label_with(lv_screen_active(), "图书馆旁散步"));
    screenshot("shahe-journal-next.ppm");
    press("×");
}
static void campus_round_trip(void)
{
    leave(PW_DORM);
    screenshot("shahe-yanbei.ppm");
    road(385, 630);
    road(485, 690);
    road(640, 690);
    road(800, 690);
    check_campus_tiles();
    screenshot("shahe-hongyan-guomai.ppm");
    assert(label_with(lv_screen_active(), "鸿雁路一带"));
    assert(!label_with(lv_screen_active(), "路标"));
    assert(!label_with(lv_screen_active(), "校园路标"));
    assert(!label_with(lv_screen_active(), "鸿雁路"));
    assert(!label_with(lv_screen_active(), "国脉西路"));
    assert(!label_with(lv_screen_active(), "国脉路"));
    press("状态");
    assert(label_with(lv_screen_active(), "小猫状态"));
    screenshot("shahe-campus-status.ppm");
    press("×");
    assert(!label_with(lv_screen_active(), "小猫状态"));
    road(945, 690);
    screenshot("shahe-teaching-gate.ppm");
    enter("教综楼", PW_CLASSROOM);
    screenshot("shahe-classroom.ppm");
    memory_demo();
    check_new_floor(340, 372);
    tap(189, 411);
    idle(PW_CLASSROOM);
    settled_at(189, 411);
    tap(88, 256);
    assert(world.settlements[PW_STUDY] == 0);
    until_action(PW_STUDY);
    step(550);
    assert(label_with(lv_screen_active(), "饱食 -7 · 心情 +7"));
    screenshot("shahe-classroom-study.ppm");
    tap(88, 256);
    assert(result == PW_BUSY && world.settlements[PW_STUDY] == 1);
    step(pw_duration(PW_STUDY));
    leave(PW_CLASSROOM);
    screenshot("shahe-teaching-entry.ppm");
    road(860, 690);
    road(800, 820);
    assert(label_with(lv_screen_active(), "国脉路南段"));
    screenshot("shahe-guomai-south.ppm");
    road(800, 950);
    road(800, 1060);
    road(646, 1060);
    screenshot("shahe-canteen-gate.ppm");
    enter("学生餐厅", PW_CANTEEN);
    screenshot("shahe-canteen.ppm");
    check_new_floor(340, 360);
    tap(261, 396);
    idle(PW_CANTEEN);
    settled_at(261, 396);
    tap(87, 252);
    assert(world.settlements[PW_EAT] == 0);
    until_action(PW_EAT);
    step(550);
    assert(label_with(lv_screen_active(), "饱食 +20"));
    screenshot("shahe-canteen-meal.ppm");
    tap(87, 252);
    assert(result == PW_BUSY && world.settlements[PW_EAT] == 1);
    step(pw_duration(PW_EAT));
    leave(PW_CANTEEN);
    screenshot("shahe-canteen-entry.ppm");
    road(770, 1060);
    road(800, 938);
    road(800, 785);
    road(800, 690);
    enter("健身房", PW_GYM);
    screenshot("shahe-gym.ppm");
    check_new_floor(320, 360);
    tap(119, 256);
    until_action(PW_WORKOUT);
    assert(world.settlements[PW_WORKOUT] == 0);
    step(700);
    screenshot("shahe-gym-workout.ppm");
    step((uint32_t)(world.action_end - world.now));
    assert(world.settlements[PW_WORKOUT] == 1);
    assert(label_with(lv_screen_active(), "饱食 -6 · 心情 +12"));
    screenshot("shahe-gym-gains.ppm");
    leave(PW_GYM);
    road(695, 690);
    road(545, 690);
    road(390, 690);
    road(385, 605);
    enter("雁北园", PW_DORM);
    assert(world.settlements[PW_EAT] == 1 && world.settlements[PW_STUDY] == 1);
    screenshot("shahe-return-home.ppm");
}
static void dorm_takeout(void)
{
    assert(world.scene == PW_DORM && world.order == PW_ORDER_NONE);
    press("外卖");
    assert(label_with(lv_screen_active(), "宿舍外卖"));
    screenshot("shahe-takeout-menu.ppm");
    press("确认下单");
    assert(world.order == PW_ORDER_DELIVERING);
    pet_ui_set_save_status(PET_SAVE_PENDING);
    assert(!label_with(lv_screen_active(), "取餐"));
    press("外卖");
    assert(!label_with(lv_screen_active(), "确认下单"));
    screenshot("shahe-takeout-delivering.ppm");
    press("×");
    step((uint32_t)(world.order_due - world.now));
    assert(world.order == PW_ORDER_READY && label_with(lv_screen_active(), "取餐"));
    screenshot("shahe-takeout-arrived.ppm");
    int food = world.food;
    press("取餐");
    until_action(PW_TAKEOUT);
    assert(world.food == food && world.order == PW_ORDER_READY);
    step((uint32_t)(world.action_end - world.now));
    assert(world.food == food + 12 && world.order == PW_ORDER_NONE && world.settlements[PW_TAKEOUT] == 1);
    assert(label_with(lv_screen_active(), "饱食 +12"));
    screenshot("shahe-takeout-gains.ppm");
    tap(100, 126);
    until_action(PW_REST);
    step(700);
    assert(world.phase == PW_SLEEP);
}
static void campus_extended_route(void)
{
    /* Use the same safe point as an outdoor save, then navigate by native taps. */
    pw_init(&world);
    world.scene = PW_CAMPUS;
    pw_point_t origin = pw_node(PW_CAMPUS, 0);
    world.x = origin.x;
    world.y = origin.y;
    world.node = 0;
    pet_ui_update(&world);
    step(2500);
    road(800, 560);
    road(800, 430);
    road(800, 295);
    road(800, 175);
    assert(label_with(lv_screen_active(), "校园北区"));
    screenshot("shahe-full-north.ppm");
    road(800, 310);
    road(800, 445);
    road(800, 575);
    road(800, 690);
    road(650, 690);
    road(500, 690);
    road(350, 690);
    road(200, 690);
    road(50, 690);
    assert(label_with(lv_screen_active(), "西门一带"));
    screenshot("shahe-full-west.ppm");
    road(210, 690);
    road(365, 690);
    road(520, 690);
    road(670, 690);
    road(800, 690);
    road(800, 825);
    road(800, 965);
    road(800, 1100);
    road(800, 1220);
    assert(label_with(lv_screen_active(), "南门一带"));
    screenshot("shahe-full-south.ppm");
    road(945, 1230);
    road(1095, 1230);
    road(1235, 1230);
    road(1340, 1125);
    road(1340, 990);
    assert(label_with(lv_screen_active(), "东区教学区"));
    screenshot("shahe-full-east.ppm");
}
int main(void)
{
    lv_init();
    lv_display_t *d = lv_display_create(390, 450);
    lv_display_set_color_format(d, LV_COLOR_FORMAT_RGB888);
    lv_display_set_buffers(d, draw_buffer, NULL, sizeof(draw_buffer), LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_flush_cb(d, flush);
    lv_indev_t *input = lv_indev_create();
    lv_indev_set_type(input, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(input, read_pointer);
    reset();
    press("手账");
    screenshot("shahe-journal-first.ppm");
    press("×");
    dorm();
    puts("[host:shahe-ui] native dorm floor, detour, modal, desk, ladder, sleep and wake PASS");
    campus_round_trip();
    dorm_takeout();
    journal_demo();
    campus_extended_route();
    unsigned expected_timers = timers();
    for (unsigned i = 0; i < 10; i++)
    {
        reset();
        assert(timers() == expected_timers);
    }
    puts("[host:shahe-ui] native journal, memory rounds, stamps, life route, glyphs and recreation PASS");
    return 0;
}
