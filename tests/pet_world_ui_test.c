/* Real production LVGL and UI, simulated pointer and clock; not physical evidence. */
#include "lvgl.h"
#include "pet_ui.h"
#include "pet_version.h"
#include "pet_world_assets.h"
#include "pet_shahe_assets.h"
#include "pet_world_font.h"
#include "src/draw/lv_draw_buf_private.h"
#include "src/draw/lv_image_decoder_private.h"
#include "src/misc/cache/instance/lv_image_cache.h"
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
static uint64_t flushed_pixels;
static lv_draw_buf_malloc_cb_t image_allocate;
static lv_draw_buf_free_cb_t image_release;
static size_t image_live, image_peak;
static void *image_pointer;
static void *track_image_allocate(size_t size, lv_color_format_t format)
{
    assert(!image_pointer);
    image_pointer = image_allocate(size, format);
    assert(image_pointer);
    image_live = size;
    if (image_live > image_peak) image_peak = image_live;
    return image_pointer;
}
static void track_image_release(void *p)
{
    assert(p == image_pointer);
    image_release(p);
    image_pointer = NULL;
    image_live = 0;
}
static void check_scene_decoders(void)
{
    const lv_image_dsc_t *plates[] = {
        &shahe_room_0, &shahe_room_1, &shahe_room_2, &shahe_room_3,
        &shahe_front_0, &shahe_front_1, &shahe_front_2, &shahe_seat_0,
        &shahe_building_0, &shahe_building_1, &shahe_building_2, &shahe_tree,
        &shahe_library, &shahe_center, &shahe_lecture, &shahe_gate,
        &shahe_decor_0, &shahe_decor_1, &shahe_decor_2, &shahe_decor_3,
        &shahe_tree_shadow
    };
    lv_draw_buf_handlers_t *handlers = lv_draw_buf_get_image_handlers();
    image_allocate = handlers->buf_malloc_cb;
    image_release = handlers->buf_free_cb;
    handlers->buf_malloc_cb = track_image_allocate;
    handlers->buf_free_cb = track_image_release;
    for (unsigned repeat = 0; repeat < 10; repeat++)
        for (unsigned i = 0; i < 46; i++)
        {
            const lv_image_dsc_t *src = i < 25 ? shahe_ground[i] : plates[i - 25];
            lv_image_decoder_dsc_t decoder;
            lv_image_decoder_args_t args = {0};
            args.no_cache = true;
            assert(lv_image_decoder_open(&decoder, src, &args) == LV_RESULT_OK);
            assert(decoder.decoded);
            assert(decoder.header.w == src->header.w && decoder.header.h == src->header.h);
            lv_image_decoder_close(&decoder);
            assert(!image_pointer && !image_live);
        }
    handlers->buf_malloc_cb = image_allocate;
    handlers->buf_free_cb = image_release;
    assert(image_peak <= 351000 + 64);
    printf("[host:assets] 46 production decoders x10, released each time; peak image allocation=%zu bytes PASS\n", image_peak);
}
static void flush(lv_display_t *d, const lv_area_t *a, uint8_t *data)
{
    int width = a->x2 - a->x1 + 1;
    flushed_pixels += (uint64_t)width * (a->y2 - a->y1 + 1);
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
        bool map_child = false, scroll_child = false;
        for (lv_obj_t *q = p; q; q = lv_obj_get_parent(q))
        {
            if (lv_obj_get_width(q) == PW_CAMPUS_WIDTH) map_child = true;
            if (lv_obj_has_flag(q, LV_OBJ_FLAG_SCROLLABLE) && lv_obj_get_width(q)==322 && lv_obj_get_height(q)==250) scroll_child = true;
        }
        if (!map_child)
        {
            assert(a.x1 >= 0 && a.x2 < 390);
            if (!scroll_child) assert(a.y1 >= 0 && a.y2 < 450);
        }
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
static struct { void *p; size_t size; } profile_allocations[128];
static unsigned profile_large_decodes;
static size_t profile_live, profile_peak;
static void *profile_allocate(size_t size, lv_color_format_t format)
{
    void *p = image_allocate(size, format);
    assert(p);
    for (unsigned i=0; i<128; i++) if (!profile_allocations[i].p) {
        profile_allocations[i].p=p; profile_allocations[i].size=size;
        profile_live+=size;
        if(profile_live>profile_peak)profile_peak=profile_live;
        if(size>=100000)profile_large_decodes++;
        return p;
    }
    assert(false); return NULL;
}
static void profile_release(void *p)
{
    for(unsigned i=0;i<128;i++)if(profile_allocations[i].p==p) {
        profile_live-=profile_allocations[i].size;
        profile_allocations[i].p=NULL;image_release(p);return;
    }
    assert(false);
}
static void render_profile(bool strict)
{
    lv_image_cache_drop(NULL);
    lv_draw_buf_handlers_t *handlers=lv_draw_buf_get_image_handlers();
    image_allocate=handlers->buf_malloc_cb;image_release=handlers->buf_free_cb;
    handlers->buf_malloc_cb=profile_allocate;handlers->buf_free_cb=profile_release;
    FILE *f=fopen("render-profile.jsonl","wb");assert(f);
    for(unsigned scene=0;scene<3;scene++) {
        reset();
        if(scene) {
            world.scene=PW_CAMPUS;world.node=0;
            pw_point_t p=pw_node(PW_CAMPUS,0);world.x=p.x;world.y=p.y;
            pet_ui_update(&world);
        }
        for(unsigned i=0;i<80;i++)step(40);
        if(scene==2)assert(pw_request(&world,PW_ENTER_DORM)==PW_OK);
        unsigned before=profile_large_decodes;
        uint64_t pixels=flushed_pixels;
        for(unsigned i=0;i<50;i++)step(40);
        unsigned count=profile_large_decodes-before;
        pixels=flushed_pixels-pixels;
        printf("[host:render-profile] scene=%u frames=50 large_decodes=%u pixels=%llu peak=%zu\n",
            scene,count,(unsigned long long)pixels,profile_peak);fflush(stdout);
        fprintf(f,"{\"scene\":%u,\"frames\":50,\"large_decodes\":%u,\"pixels\":%llu,\"peak_image_bytes\":%zu}\n",
            scene,count,(unsigned long long)pixels,profile_peak);fflush(f);
        if(strict) {
            assert(count<=(scene==2?18U:0U));
            if(!scene)assert(pixels<390U*450U*10U);
        }
    }
    fclose(f);
    lv_image_cache_drop(NULL);
    assert(profile_live==0);
    assert(profile_peak<=LV_CACHE_DEF_SIZE+351000+4096);
    handlers->buf_malloc_cb=image_allocate;handlers->buf_free_cb=image_release;
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
    tap(370, 440); /* Outside the expanded settings dialog. */
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
    assert(label_with(lv_screen_active(), "参与奖励 +0 金币"));
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
static void shop_while_asleep(void)
{
    assert(world.phase == PW_SLEEP);
    unsigned coins = world.wallet.coins;
    press("布置"); assert(label_with(lv_screen_active(), "布置宿舍"));
    assert(label_with(lv_screen_active(), "先叫醒"));
    screenshot("shahe-shop-asleep.ppm"); press("×");
    tap(100,126); step(600); assert(world.phase == PW_IDLE && world.wallet.coins == coins);
    assert(!label_with(lv_screen_active(), "手账"));
}
static lv_obj_t *find_scrollable_box(lv_obj_t *p, int w, int h)
{
    for (unsigned i = 0; i < lv_obj_get_child_count(p); i++)
    {
        lv_obj_t *o = lv_obj_get_child(p, i);
        if (lv_obj_has_flag(o, LV_OBJ_FLAG_SCROLLABLE) && lv_obj_get_width(o) == w && lv_obj_get_height(o) == h)
            return o;
        lv_obj_t *f = find_scrollable_box(o, w, h);
        if (f) return f;
    }
    return NULL;
}
static void album_demo(void)
{
    char progress[64];
    snprintf(progress, sizeof(progress), "已尝 %u / 12 道 · 吃过才会点亮", pw_dish_count(&world));
    press("图鉴");
    assert(label_with(lv_screen_active(), "校园图鉴"));
    assert(label_with(lv_screen_active(), progress));
    assert(label_with(lv_screen_active(), "未解锁 · 去食堂或点外卖"));
    assert(label_with(lv_screen_active(), "在食堂点餐，或在宿舍点外卖"));
    screenshot("shahe-album.ppm");
    lv_obj_t *list = find_scrollable_box(lv_screen_active(), 322, 296);
    assert(list && lv_obj_get_scroll_y(list) == 0);
    lv_obj_scroll_to_y(list, 300, LV_ANIM_OFF);
    step(40);
    assert(lv_obj_get_scroll_y(list) > 0);
    screenshot("shahe-album-scrolled.ppm");
    press("×");
    assert(!label_with(lv_screen_active(), "校园图鉴"));
    assert(world.phase == PW_IDLE);
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
    assert(label_with(lv_screen_active(), "饱食 -7 · 心情 +4"));
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
    for(unsigned i=0;i<1000 && world.phase==PW_WALK;i++) step(20);
    assert(world.phase==PW_SEATED && world.settlements[PW_EAT]==0); step(550);
    screenshot("shahe-canteen-seated-menu.ppm");
    press("确认用餐 · 6金币");
    assert(world.phase==PW_ACTION);
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
    assert(label_with(lv_screen_active(), "饱食 -6 · 心情 +6"));
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
    press("确认下单 · 8金币");
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
    assert(label_with(lv_screen_active(), "饱食 +12 · 心情 +1"));
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
static void real_clock_previews(void)
{
    const char *at[]={"20261108070000","20261108120000","20261108180000","20261108230000"};
    const char *name[]={"morning","day","dusk","night"};
    for(unsigned outside=0;outside<2;outside++) {
        reset();world.now=5000;
        if(outside){world.scene=PW_CAMPUS;world.node=0;pw_point_t p=pw_node(PW_CAMPUS,0);world.x=p.x;world.y=p.y;}
        for(unsigned i=0;i<4;i++) {
            uint32_t utc;assert(pt_parse((const uint8_t *)at[i],14,&utc));
            world.clock=(pt_clock_t){utc,true,true,PT_SYNCED,PT_OK};pet_ui_update(&world);step(40);
            char file[80];snprintf(file,sizeof(file),"shahe-clock-%s-%s.ppm",outside?"campus":"dorm",name[i]);screenshot(file);
            if(i==3)assert(label_with(lv_screen_active(),"有点困，回床上休息吧"));
        }
        press("状态");assert(label_with(lv_screen_active(),"2026-11-08 · 手机校时"));
        assert(label_with(lv_screen_active(),"v" PET_FIRMWARE_VERSION));screenshot(outside?"shahe-clock-campus-status.ppm":"shahe-clock-dorm-status.ppm");press("×");
        if(!outside){
            tap(189,411);idle(PW_DORM);settled_at(189,411);
            press("出门");assert(result==PW_CURFEW && world.scene==PW_DORM);
            assert(label_with(lv_screen_active(),"宿舍宵禁"));screenshot("shahe-curfew-popup.ppm");
            press("留在宿舍");step(500);assert(!label_with(lv_screen_active(),"宿舍宵禁"));
            assert(pt_parse((const uint8_t *)"20261108060000",14,&world.clock.utc));pet_ui_update(&world);
            leave(PW_DORM);assert(world.scene==PW_CAMPUS && world.clock.valid);
        }
    }
    world.clock.valid=false;world.clock.state=PT_ERROR;pet_ui_update(&world);step(40);
    assert(label_with(lv_screen_active(),"--:-- 待校时"));press("状态");
    assert(label_with(lv_screen_active(),"校时失败，请用手机重试"));screenshot("shahe-clock-error.ppm");press("×");
    world.clock=(pt_clock_t){0};
    pet_link_t auto_link={PET_LINK_CONNECTED,"ShaheCat-800CD1",1};pet_ui_set_link(&auto_link);pet_ui_update(&world);press("状态");
    assert(label_with(lv_screen_active(),"等待手机自动校时"));screenshot("shahe-auto-wait.ppm");
    auto_link.auto_time=2;pet_ui_set_link(&auto_link);pet_ui_update(&world);
    assert(label_with(lv_screen_active(),"自动校时未完成，可重连"));screenshot("shahe-auto-unavailable.ppm");
    uint32_t utc;assert(pt_parse((const uint8_t *)"20261108120000",14,&utc));
    world.clock=(pt_clock_t){utc,true,true,PT_SYNCED,PT_OK,true};pet_ui_update(&world);
    assert(label_with(lv_screen_active(),"2026-11-08 · 自动校时"));screenshot("shahe-auto-synced.ppm");press("×");
    auto_link.state=PET_LINK_STARTING;auto_link.auto_time=0;pet_ui_set_link(&auto_link);
    reset();
    puts("[host:clock-ui] four light bands indoor/outdoor, date/version, bedtime and lost clock PASS");
}
static void new_life_and_challenges(void)
{
    const pw_scene_t rooms[] = {PW_CANTEEN, PW_GYM, PW_DORM};
    const char *entries[] = {"配餐", "节奏", "找物"};
    const char *food[] = {"米饭", "青菜", "鸡蛋", "热汤"};
    const char *slots[] = {"位置一", "位置二", "位置三", "位置四"};
    for (unsigned k = 0; k < 3; k++)
    {
        reset(); world.scene = rooms[k];
        pw_point_t p = pw_node(world.scene, 3); world.x = p.x; world.y = p.y;
        pet_ui_update(&world); step(40);
        press(entries[k]); assert(result == PW_OK);
        for (unsigned n = 0; n < 500 && world.phase == PW_WALK; n++) step(40);
        assert(world.phase == PW_MINIGAME);
        screenshot(k == 0 ? "shahe-meal-input.ppm" : k == 1 ? "shahe-rhythm-wait.ppm" : "shahe-find-reveal.ppm");
        if (k == 2) { step(5000); screenshot("shahe-find-input.ppm"); }
        if (k == 1)
        {
            for (unsigned n = 0; n < 6; n++)
            {
                unsigned at = world.challenge.elapsed % 3000;
                assert(at < 1500); step(1500 - at);
                if (n == 0) screenshot("shahe-rhythm-green.ppm");
                press("跟上节拍");
                assert(world.challenge.score == n + 1);
                step(3000 - world.challenge.elapsed % 3000);
            }
        }
        else for (unsigned n = 0; n < 3; n++)
        {
            unsigned pick = world.challenge.targets[n];
            if (k == 2) for (unsigned j = 0; j < 4; j++)
                if (world.challenge.cards[j] == world.challenge.targets[n]) pick = j;
            press(k == 0 ? food[pick] : slots[pick]);
        }
        assert(world.challenge.phase == PG_WON && world.memory_wins == 1 && world.mood == 63);
        assert(label_with(lv_screen_active(), "金币 +10 · 心情 +3"));
        if (k == 0)
        {
            world.mood_gain = 0; pet_ui_update(&world);
            assert(label_with(lv_screen_active(), "金币 +10 · 心情 +0"));
            world.mood_gain = 3; pet_ui_update(&world);
        }
        screenshot(k == 0 ? "shahe-meal-won.ppm" : k == 1 ? "shahe-rhythm-won.ppm" : "shahe-find-won.ppm");
        press("×"); assert(world.phase == PW_IDLE);
        press(entries[k]); assert(result == PW_COOLDOWN && world.memory_wins == 1);
        world.cooldown[PW_MEMORY] = 0;
        press(entries[k]);
        for (unsigned n = 0; n < 500 && world.phase == PW_WALK; n++) step(40);
        assert(world.phase == PW_MINIGAME);
        press("×"); assert(world.phase == PW_IDLE && world.memory_wins == 1);
    }
    for (pw_scene_t scene = PW_DORM; scene <= PW_GYM; scene++)
    {
        if (scene == PW_CAMPUS) continue;
        reset(); world.scene = scene;
        pw_point_t p = pw_node(scene, 3); world.x = p.x; world.y = p.y;
        pet_ui_update(&world); step(40);
        press("看看"); assert(result == PW_OK);
        for (unsigned n = 0; n < 500 && world.phase == PW_WALK; n++) step(40);
        assert(world.phase == PW_ACTION && world.mood == 60);
        char name[64]; snprintf(name, sizeof(name), "shahe-life-%u.ppm", scene); screenshot(name);
        step(4000); assert(world.phase == PW_IDLE && world.mood == 62 && !world.memory_wins);
    }
    for (unsigned mask = 0; mask < 8; mask++)
    {
        reset(); world.wallet = (pe_wallet_t){300, (uint8_t)mask, (uint8_t)mask};
        pet_ui_update(&world); step(40);
        press("布置");
        assert(label_with(lv_screen_active(), "窗台盆栽"));
        assert(label_with(lv_screen_active(), "校园挂画"));
        assert(label_with(lv_screen_active(), "桌面书架"));
        char name[64]; snprintf(name, sizeof(name), "shahe-shop-owned-%u.ppm", mask); screenshot(name);
        press("×"); assert(world.wallet.coins == 300 && world.wallet.owned == mask);
    }
    reset(); world.wallet.coins = 200; pet_ui_update(&world); step(40);
    const unsigned pixels[][2] = {{204,193},{107,180},{47,214}};
    unsigned char old_pixel[3][3]; lv_refr_now(NULL);
    for (unsigned i=0; i<3; i++) memcpy(old_pixel[i], frame[pixels[i][1]][pixels[i][0]], 3);
    press("布置");
    const char *names[] = {"窗台盆栽", "校园挂画", "桌面书架"};
    for (unsigned i=0; i<3; i++)
    {
        lv_obj_t *row = lv_obj_get_parent(label_with(lv_screen_active(), names[i]));
        lv_obj_t *button = NULL;
        for (unsigned j=0; j<lv_obj_get_child_count(row); j++)
            if (lv_obj_check_type(lv_obj_get_child(row,j), &lv_button_class)) button = lv_obj_get_child(row,j);
        assert(button); lv_area_t area; lv_obj_get_coords(button,&area);
        tap((area.x1+area.x2)/2,(area.y1+area.y2)/2);
    }
    assert(world.wallet.coins == 20 && world.wallet.owned == 7 && world.wallet.equipped == 7);
    screenshot("shahe-shop-purchased.ppm"); press("×"); screenshot("shahe-room-furnished.ppm");
    for (unsigned i=0; i<3; i++) assert(memcmp(old_pixel[i], frame[pixels[i][1]][pixels[i][0]],3));
    press("布置");
    for (unsigned i=0; i<3; i++) press("收起");
    assert(world.wallet.coins == 20 && world.wallet.owned == 7 && world.wallet.equipped == 0);
    screenshot("shahe-shop-stored.ppm");
    for (unsigned i=0; i<3; i++) press("摆上");
    assert(world.wallet.coins == 20 && world.wallet.equipped == 7);
    press("×");
    world.wallet=(pe_wallet_t){0,0,0}; pet_ui_update(&world); press("布置");
    assert(label_with(lv_screen_active(), "金币不足")); screenshot("shahe-decor-poor.ppm"); press("×");
    puts("[host:shahe-ui] three challenges, coin rewards, four life corners, decor8 ownership states, purchases/free toggles and visible3 furniture PASS");

}

static void daily_checkin_ui(void)
{
    reset(); press("状态"); assert(label_with(lv_screen_active(),"先校时"));
    press("先校时"); assert(world.wallet.coins==30&&!world.checkin_day);
    screenshot("shahe-checkin-unset.ppm");
    pt_date_t date={2026,11,8,23,59,59};uint32_t utc;assert(pt_encode(&date,&utc));
    world.clock=(pt_clock_t){utc,true,true,PT_SYNCED,PT_OK};pet_ui_update(&world);
    assert(label_with(lv_screen_active(),"签到 +6"));screenshot("shahe-checkin-ready.ppm");
    press("签到 +6");assert(world.wallet.coins==36&&world.checkin_day);
    assert(label_with(lv_screen_active(),"今日已领"));screenshot("shahe-checkin-claimed.ppm");
    unsigned day=world.checkin_day;press("今日已领");assert(world.wallet.coins==36&&world.checkin_day==day);
    world.clock.utc++;pet_ui_update(&world);press("签到 +6");
    assert(world.wallet.coins==42&&world.checkin_day==day+1);
    world.clock.utc-=86400;pet_ui_update(&world);
    assert(label_with(lv_screen_active(),"校准日期"));screenshot("shahe-checkin-rollback.ppm");
    world.checkin_day=0;world.wallet.coins=9999;pet_ui_update(&world);
    assert(label_with(lv_screen_active(),"余额已满"));screenshot("shahe-checkin-full.ppm");
    world.wallet.coins=9997;pet_ui_update(&world);press("签到 +6");
    assert(world.wallet.coins==9999&&world.coin_gain==2&&label_with(lv_screen_active(),"今日已领"));
    press("×");
    puts("[host:daily-ui] real pointer wait/trusted/claim/disabled repeat/Beijing midnight/rollback/full/actual cap PASS");
}

static void swipe_up(void)
{
    pointer.x=180;pointer.y=315;pointer_state=LV_INDEV_STATE_PRESSED;step(40);
    for(int y=300;y>=130;y-=10){pointer.y=y;step(20);}
    pointer_state=LV_INDEV_STATE_RELEASED;step(40);step(400);
}
static void scroll_menus_and_curfew(void)
{
    reset();world.scene=PW_CANTEEN;world.wallet.coins=100;world.food=40;
    pw_point_t p=pw_node(PW_CANTEEN,3);world.x=p.x;world.y=p.y;pet_ui_update(&world);
    tap(87,252);for(unsigned i=0;i<1000&&world.phase==PW_WALK;i++)step(20);
    assert(world.phase==PW_SEATED && world.wallet.coins==100 && world.food==40);step(550);
    assert(label_with(lv_screen_active(),"坐下慢慢选餐"));screenshot("shahe-meal-menu-top.ppm");
    tap(60,220);assert(label_with(lv_screen_active(),"确认用餐 · 7金币") && world.wallet.coins==100);
    swipe_up();swipe_up();
    lv_obj_t *last=control("红豆小点心");lv_area_t a;lv_obj_get_coords(last,&a);
    assert(a.y1>=96 && a.y2<346);screenshot("shahe-meal-menu-bottom.ppm");
    press("红豆小点心");assert(world.wallet.coins==100 && world.food==40);
    press("确认用餐 · 3金币");assert(world.wallet.coins==97 && world.food==50 && world.mood==61 && world.settlements[PW_EAT]==1);
    step(6500);tap(87,252);for(unsigned i=0;i<1000&&world.phase==PW_WALK;i++)step(20);
    assert(world.phase==PW_SEATED);step(550);press("×");assert(world.phase==PW_IDLE && world.wallet.coins==97);
    reset();world.wallet.coins=100;world.food=40;pet_ui_update(&world);press("外卖");
    swipe_up();swipe_up();press("暖胃小米粥");screenshot("shahe-takeout-menu-bottom.ppm");
    press("确认下单 · 5金币");assert(world.wallet.coins==95 && world.order_choice==6);
    press("外卖");assert(label_with(lv_screen_active(),"宿舍外卖"));screenshot("shahe-takeout-congee-tracking.ppm");press("×");
    reset();uint32_t utc;assert(pt_parse((const uint8_t *)"20261109230000",14,&utc));world.clock=(pt_clock_t){utc,true,true,PT_SYNCED,PT_OK};pet_ui_update(&world);
    press("出门");assert(result==PW_CURFEW && world.scene==PW_DORM);press("留在宿舍");
    tap(100,126);until_action(PW_REST);step(700);assert(world.phase==PW_SLEEP);
    tap(100,126);step(600);assert(world.phase==PW_IDLE);
    press("出门");assert(result==PW_CURFEW);screenshot("shahe-curfew-after-wake.ppm");press("留在宿舍");
    world.wallet=(pe_wallet_t){200,7,7};pet_ui_update(&world);screenshot("shahe-dorm-furnished-night.ppm");
    assert(!label_with(lv_screen_active(),"商店"));
    puts("[host:menu-ui] native pointer two scrolls to sixth meals, no charge on selection/cancel, seated confirmation, delivery kind, curfew/wake/close PASS");
}

int main(int argc, char **argv)
{
    lv_init();
    check_scene_decoders();
    lv_display_t *d = lv_display_create(390, 450);
    lv_display_set_color_format(d, LV_COLOR_FORMAT_RGB888);
    lv_display_set_buffers(d, draw_buffer, NULL, sizeof(draw_buffer), LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_flush_cb(d, flush);
    lv_indev_t *input = lv_indev_create();
    lv_indev_set_type(input, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(input, read_pointer);
    bool measure=argc>1 && !strcmp(argv[1],"--measure");
    render_profile(!measure);
    if(measure)return 0;
    reset();
    lv_obj_t *entry=control("状态");lv_area_t entry_area;
    lv_obj_get_coords(entry,&entry_area);
    assert(entry_area.x1==296 && entry_area.y1==28 && entry_area.x2<=357);
    assert(lv_obj_get_width(entry)==62 && lv_obj_get_height(entry)==62);
    tap(entry_area.x1+4,(entry_area.y1+entry_area.y2)/2);
    assert(label_with(lv_screen_active(),"小猫状态"));press("×");
    tap(282,59); assert(!label_with(lv_screen_active(), "布置宿舍"));
    assert(!label_with(lv_screen_active(), "商店"));
    press("布置"); screenshot("shahe-decor-first.ppm"); press("×");
    press("状态");
    assert(world.sound_mode == 2 && world.volume == 5);
    screenshot("shahe-audio-music.ppm");
    press("背景音乐：开");
    assert(world.sound_mode == 1);
    screenshot("shahe-audio-effects.ppm");
    press("背景音乐：关");
    assert(world.sound_mode == 2);
    press("−");
    assert(world.volume == 4 && label_with(lv_screen_active(), "音量 4 / 10"));
    for (unsigned i=0; i<12; i++) press("−");
    assert(world.volume == 0 && label_with(lv_screen_active(), "音量 0 / 10"));
    screenshot("shahe-audio-zero.ppm");
    for (unsigned i=0; i<14; i++) press("+");
    assert(world.volume == 10 && label_with(lv_screen_active(), "音量 10 / 10"));
    screenshot("shahe-audio-full.ppm");
    pet_link_t link = {PET_LINK_WAITING, "ShaheCat-800CD1"};
    pet_ui_set_link(&link); pet_ui_update(&world);
    assert(label_with(lv_screen_active(), "蓝牙等待连接"));
    link.state=PET_LINK_CONNECTED;
    pet_ui_set_link(&link); pet_ui_update(&world);
    assert(label_with(lv_screen_active(), "蓝牙已连接"));
    assert(label_with(lv_screen_active(), "ShaheCat-800CD1"));
    screenshot("shahe-link-connected.ppm");
    link.state=PET_LINK_ERROR;
    pet_ui_set_link(&link); pet_ui_update(&world);
    assert(label_with(lv_screen_active(), "蓝牙异常"));
    screenshot("shahe-link-error.ppm");
    link.state=PET_LINK_STARTING;
    pet_ui_set_link(&link); pet_ui_update(&world);
    pet_ui_set_audio_available(false);
    pet_ui_update(&world);
    assert(label_with(lv_screen_active(), "声音暂不可用，切换重试"));
    screenshot("shahe-audio-unavailable.ppm");
    pet_ui_set_audio_available(true);
    press("×");
    assert(!label_with(lv_screen_active(),"蓝牙启动中"));
    screenshot("shahe-audio-room.ppm");
    dorm();
    puts("[host:shahe-ui] native dorm floor, detour, modal, desk, ladder, sleep and wake PASS");
    campus_round_trip();
    dorm_takeout();
    shop_while_asleep();
    album_demo();
    campus_extended_route();
    real_clock_previews();
    new_life_and_challenges();
    daily_checkin_ui();
    scroll_menus_and_curfew();
    unsigned expected_timers = timers();
    for (unsigned i = 0; i < 10; i++)
    {
        reset();
        assert(timers() == expected_timers);
    }
    puts("[host:shahe-ui] decor, memory rounds, paid life route, glyphs and recreation PASS");
    return 0;
}
