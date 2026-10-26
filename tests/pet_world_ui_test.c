/* Real production LVGL and UI, simulated pointer and clock; not physical evidence. */
#include "lvgl.h"
#include "pet_ui.h"
#include "pet_world_assets.h"
#include "pet_world_font.h"
#include <stdio.h>
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
static lv_obj_t *label_with(lv_obj_t *p, const char *name)
{
    for (unsigned i = 0; i < lv_obj_get_child_count(p); i++)
    {
        lv_obj_t *o = lv_obj_get_child(p, i);
        if (lv_obj_check_type(o, &lv_label_class) && !strcmp(lv_label_get_text(o), name))
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
    assert(lv_obj_check_type(p, &lv_button_class));
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
static void reset(void)
{
    pw_init(&world);
    pet_ui_create(command);
    pet_ui_update(&world);
    lv_obj_update_layout(lv_screen_active());
}
static void until_action(void)
{
    for (int i = 0; i < 3000 && world.phase == PW_WALK; i++)
        step(20);
    assert(world.phase == PW_ACTION);
}
static void travel(const char *name, pw_scene_t scene)
{
    press("出行");
    press(name);
    for (int i = 0;
         i < 1500 && (world.scene != scene || world.phase == PW_WALK || world.phase == PW_TRANSITION); i++)
        step(20);
    assert(world.scene == scene);
}
int main(void)
{
    lv_init();
    lv_display_t *d = lv_display_create(390, 450);
    lv_display_set_color_format(d, LV_COLOR_FORMAT_RGB888);
    lv_display_set_buffers(d, draw_buffer, NULL, sizeof(draw_buffer), LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_flush_cb(d, flush);
    lv_indev_t *i = lv_indev_create();
    lv_indev_set_type(i, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(i, read_pointer);
    reset();
    unsigned count = timers();
    screenshot("world-dorm.ppm");
    press("状态");
    assert(shown(label_with(lv_screen_active(), "小猫状态")));
    screenshot("world-status.ppm");
    unsigned touch = world.settlements[PW_TOUCH];
    tap(327, 154);
    assert(world.settlements[PW_TOUCH] == touch &&
           world.target == PW_NONE); /* scrim consumes furniture tap */
    tap(327, 154);
    assert(world.target == PW_CODE && world.food == 70);
    until_action();
    assert(world.food == 67 && world.mood == 64);
    step(700);
    screenshot("world-code.ppm");
    press("状态");
    uint64_t now = world.now;
    step(500);
    assert(world.now == now + 500 && world.action == PW_CODE);
    press("出行");
    assert(lv_obj_has_state(control("教室"), LV_STATE_DISABLED));
    screenshot("world-busy-travel.ppm");
    tap(20, 200);
    step(11000);
    travel("教室", PW_CLASSROOM);
    assert(world.food == 67);
    screenshot("world-classroom.ppm");
    tap(143, 249);
    until_action();
    assert(world.food == 62 && world.mood == 69);
    step(700);
    screenshot("world-study.ppm");
    step(9000);
    travel("食堂", PW_CANTEEN);
    screenshot("world-canteen.ppm");
    tap(110, 277);
    until_action();
    assert(world.food == 77);
    step(700);
    screenshot("world-eat.ppm");
    step(5000);
    travel("宿舍", PW_DORM);
    tap(43, 247);
    until_action();
    step(700);
    assert(world.phase == PW_SLEEP);
    screenshot("world-sleep.ppm");
    press("状态");
    step(60000);
    assert(world.mood == 70);
    screenshot("world-sleep-status.ppm");
    tap(20, 200);
    tap(43, 247);
    step(600);
    assert(world.phase != PW_SLEEP);
    screenshot("world-wake.ppm");
    for (int n = 0; n < 10; n++)
    {
        uint8_t f = world.food, m = world.mood;
        pet_ui_create(command);
        pet_ui_update(&world);
        lv_obj_update_layout(lv_screen_active());
        assert(timers() == count && world.food == f && world.mood == m);
    }
    reset();
    world.food = 7;
    pet_ui_update(&world);
    tap(327, 154);
    assert(result == PW_LOW_FOOD && world.food == 7);
    assert(label_with(lv_screen_active(), "肚子饿了，先去食堂"));
    screenshot("world-low-food.ppm");
    reset();
    step(2400);
    for (unsigned f = 0; f < 8; f++)
    {
        world.phase = PW_WALK;
        world.facing = (uint8_t)f;
        world.now = 2520;
        world.x = 211;
        world.y = 339;
        pet_ui_update(&world);
        char filename[32];
        snprintf(filename, sizeof(filename), "world-angle-%u.ppm", f);
        screenshot(filename);
    }
    reset();
    assert(world.food == 70 && world.mood == 60 && world.scene == PW_DORM);
    screenshot("world-restart.ppm");
    puts(
        "[host:world-ui] actual LVGL: three rooms, 8 views, native pointer hotspots/panels/scrim, arrival "
        "settlement, seats/bed/wake, glyphs, targets, 10 recreations and timer count PASS; no board verdict");
    return 0;
}
