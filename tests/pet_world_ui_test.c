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
            if (lv_obj_get_width(q) == 1120)
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
static void dorm(void)
{
    assert(world.scene == PW_DORM);
    assert(!label_with(lv_screen_active(), "路标"));
    screenshot("shahe-dorm.ppm");
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
    dorm();
    puts("[host:shahe-ui] native dorm floor, detour, modal, desk, ladder, sleep and wake PASS");
    return 0;
}
