/* Room views and continuous campus camera. Model owns movement and settlement. */
#include "pet_ui.h"
#include "lvgl.h"
#include "pet_version.h"
#include "pet_world_assets.h"
#include "pet_shahe_assets.h"
#include "pet_world_font.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

#define INK 0x304C4A
#define BLUE 0x456B77
#define PAPER 0xF8F3DF
enum
{
    TAG_STATUS = 100,
    TAG_TRAVEL,
    TAG_CLOSE
};
static struct
{
    pet_ui_action_fn action;
    pet_ui_point_fn point;
    lv_obj_t *campus, *gates, *door, *location, *location_text, *goal;
    pet_world_t world;
    int panel, scene;
    lv_obj_t *background, *shadow, *sprite, *cat_hit, *foreground, *marks, *hotspot[2];
    lv_obj_t *layer, *dialog, *status, *travel, *food, *mood, *food_fill, *mood_fill, *state, *title,
        *save_note;
    lv_obj_t *corner[2], *bubble, *bubble_text;
} ui;
static unsigned save_state = PET_SAVE_PENDING;
static const char *save_notes[] = {"等待自动保存", "已保存，重启可恢复", "存档暂不可用", "保存失败，稍后重试"};
void pet_ui_set_save_status(unsigned status)
{
    save_state = status <= PET_SAVE_FAILED ? status : PET_SAVE_UNAVAILABLE;
    if (ui.save_note) lv_label_set_text(ui.save_note, save_notes[save_state]);
}
static void paint(void);
void pet_ui_set_point_handler(pet_ui_point_fn handler)
{
    ui.point = handler;
}
static void ground_click(lv_event_t *e)
{
    (void)e;
    if (ui.panel || !ui.point)
        return;
    lv_indev_t *indev = lv_indev_active();
    if (!indev)
        return;
    lv_point_t p;
    lv_indev_get_point(indev, &p);
    float x, y;
    pw_camera(&ui.world, &x, &y);
    ui.point(p.x + (int)lroundf(x), p.y + (int)lroundf(y));
}
static lv_obj_t *box(lv_obj_t *parent, int x, int y, int w, int h, uint32_t color, int radius)
{
    lv_obj_t *o = lv_obj_create(parent);
    lv_obj_remove_style_all(o);
    lv_obj_remove_flag(o, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_pos(o, x, y);
    lv_obj_set_size(o, w, h);
    lv_obj_set_style_bg_color(o, lv_color_hex(color), 0);
    lv_obj_set_style_bg_opa(o, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(o, radius, 0);
    return o;
}
static lv_obj_t *text(lv_obj_t *parent, const char *s, int x, int y, const lv_font_t *font, uint32_t color)
{
    lv_obj_t *o = lv_label_create(parent);
    lv_obj_remove_flag(o, LV_OBJ_FLAG_CLICKABLE);
    lv_label_set_text(o, s);
    lv_obj_set_pos(o, x, y);
    lv_obj_set_style_text_font(o, font, 0);
    lv_obj_set_style_text_color(o, lv_color_hex(color), 0);
    return o;
}
static void visible(lv_obj_t *o, bool show)
{
    if (show)
        lv_obj_remove_flag(o, LV_OBJ_FLAG_HIDDEN);
    else
        lv_obj_add_flag(o, LV_OBJ_FLAG_HIDDEN);
}
static void click(lv_event_t *e)
{
    lv_obj_t *o = lv_event_get_target(e);
    if (lv_obj_has_state(o, LV_STATE_DISABLED))
        return;
    int cmd = (int)(uintptr_t)lv_event_get_user_data(e);
    if (cmd == TAG_STATUS || cmd == TAG_TRAVEL)
    {
        int p = cmd == TAG_STATUS ? 1 : 2;
        ui.panel = ui.panel == p ? 0 : p;
        paint();
        return;
    }
    if (cmd == TAG_CLOSE)
    {
        ui.panel = 0;
        paint();
        return;
    }
    if (!ui.panel && ui.action)
        ui.action((pw_command_t)cmd);
}
static lv_obj_t *button(lv_obj_t *parent, int x, int y, int w, int h, int tag, uint32_t color, int radius)
{
    lv_obj_t *o = lv_button_create(parent);
    lv_obj_remove_style_all(o);
    lv_obj_remove_flag(o, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_pos(o, x, y);
    lv_obj_set_size(o, w, h);
    lv_obj_set_style_bg_color(o, lv_color_hex(color), 0);
    lv_obj_set_style_bg_opa(o, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(o, radius, 0);
    lv_obj_set_style_bg_color(o, lv_color_hex(0xDCEAF3), LV_STATE_PRESSED);
    lv_obj_set_style_opa(o, LV_OPA_50, LV_STATE_DISABLED);
    lv_obj_add_event_cb(o, click, LV_EVENT_CLICKED, (void *)(uintptr_t)tag);
    return o;
}
static void image(lv_obj_t *parent, const lv_image_dsc_t *src, int x, int y)
{
    lv_obj_t *o = lv_image_create(parent);
    lv_image_set_src(o, src);
    lv_obj_set_pos(o, x, y);
    lv_obj_remove_flag(o, LV_OBJ_FLAG_CLICKABLE);
}
static const lv_image_dsc_t *const campus_building[] = {
    &shahe_building_0, &shahe_building_1, &shahe_building_2};
static const char *const campus_names[] = {"雁北园", "教综楼", "学生餐厅"};

static void gate_badge(lv_obj_t *parent, const char *name, int x, int y, int width)
{
    lv_obj_t *sign = box(parent, x, y, width, 28, PAPER, 7);
    box(sign, 7, 11, 6, 6, 0xBF795F, 3);
    text(sign, name, 19, 2, &pet_font_16, INK);
}

static void build_campus(lv_obj_t *screen)
{
    ui.campus = box(screen, 0, 0, PW_CAMPUS_WIDTH, PW_CAMPUS_HEIGHT, 0xB9CEA2, 0);
    lv_obj_add_flag(ui.campus, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(ui.campus, ground_click, LV_EVENT_CLICKED, NULL);
    for (unsigned y = 0; y < 4; y++)
        for (unsigned x = 0; x < 4; x++)
            image(ui.campus, shahe_ground[y * 4 + x], x * 280, y * 260);
    static const int16_t trees[][2] = {
        {32,190}, {49,293}, {243,215}, {303,262}, {448,257}, {598,262},
        {690,256}, {853,274}, {197,424}, {310,466}, {420,434}, {615,453},
        {651,545}, {929,489}, {932,575}, {626,690}, {677,794}, {477,817},
        {249,765}, {287,860}, {522,879}};
    for (unsigned i = 0; i < sizeof(trees) / sizeof(trees[0]); i++)
    {
        int x = trees[i][0], y = trees[i][1];
        int h = shahe_tree.header.h;
        image(ui.campus, &shahe_tree_shadow, x + 18, y + h - 12);
        image(ui.campus, &shahe_tree, x, y);
    }
    for (unsigned room = 0; room < 3; room++)
    {
        pw_point_t door = pw_node(PW_CAMPUS, 13 + room);
        const lv_image_dsc_t *art = campus_building[room];
        int x = (int)door.x, y = (int)door.y;
        int art_x = x - art->header.w * 43 / 100;
        int art_y = room == PW_DORM ? y - art->header.h : y + 18;
        lv_obj_t *shadow = box(ui.campus, art_x + 16, art_y + art->header.h - 14,
                               art->header.w - 7, 26, 0x526746, 13);
        lv_obj_set_style_bg_opa(shadow, LV_OPA_20, 0);
        image(ui.campus, art, art_x, art_y);
        if (room != PW_DORM)
        {
            /* The route reaches the north-side entry; the illustration shows the main facade. */
            box(ui.campus, x - 21, y + 4, 42, 23, 0xB6A485, 6);
            box(ui.campus, x - 18, y + 3, 36, 15, 0xE8D3AB, 4);
            box(ui.campus, x - 14, y + 7, 28, 3, 0xF6E8C9, 1);
        }
    }
    text(ui.campus, "国脉西路", 80, 385, &pet_font_16, INK);
    text(ui.campus, "鸿雁路", 410, 379, &pet_font_16, INK);
    text(ui.campus, "国脉路", 622, 618, &pet_font_16, INK);
    ui.goal = box(ui.campus, 0, 0, 20, 20, BLUE, LV_RADIUS_CIRCLE);
    lv_obj_set_style_bg_opa(ui.goal, LV_OPA_20, 0);
    lv_obj_set_style_border_width(ui.goal, 2, 0);
    lv_obj_set_style_border_color(ui.goal, lv_color_hex(BLUE), 0);
}

static void build_gates(lv_obj_t *screen)
{
    ui.gates = box(screen, 0, 0, PW_CAMPUS_WIDTH, PW_CAMPUS_HEIGHT, PAPER, 0);
    lv_obj_set_style_bg_opa(ui.gates, LV_OPA_TRANSP, 0);
    for (unsigned room = 0; room < 3; room++)
    {
        pw_point_t door = pw_node(PW_CAMPUS, 13 + room);
        int y = (int)door.y - (room == PW_DORM ? 66 : 44);
        lv_obj_t *gate = button(ui.gates, (int)door.x - 48, y, 96, 70,
                                PW_ENTER_DORM + room, PAPER, 5);
        lv_obj_set_style_bg_opa(gate, LV_OPA_TRANSP, 0);
        lv_obj_set_style_bg_opa(gate, LV_OPA_20, LV_STATE_PRESSED);
        gate_badge(gate, campus_names[room], 3, 2, 90);
    }
}
typedef struct
{
    int x, y, w, h, badge_x, badge_y;
    pw_command_t command;
    unsigned icon;
} furniture_hotspot_t;

static const furniture_hotspot_t furniture[3][2] = {
    {{24, 110, 134, 61, 110, 122, PW_REST, 2}, {31, 175, 115, 80, 109, 208, PW_CODE, 3}},
    {{44, 235, 90, 75, 111, 245, PW_STUDY, 4}},
    {{36, 231, 105, 83, 117, 243, PW_EAT, 5}}
};

static const lv_image_dsc_t *const room_art[] = {&shahe_room_0, &shahe_room_1, &shahe_room_2};
static const char *const room_names[] = {"雁北园 · 宿舍", "教综楼 · 教室", "学生餐厅 · 食堂"};

static void build_hotspots(void)
{
    for (int i = 0; i < 2; i++)
        if (ui.hotspot[i])
        {
            lv_obj_delete(ui.hotspot[i]);
            ui.hotspot[i] = NULL;
        }
    if (ui.world.scene == PW_CAMPUS)
        return;
    unsigned count = ui.world.scene == PW_DORM ? 2 : 1;
    for (unsigned t = 0; t < count; t++)
    {
        const furniture_hotspot_t *f = &furniture[ui.world.scene][t];
        lv_obj_t *o = button(lv_screen_active(), f->x, f->y, f->w, f->h, f->command, PAPER, 0);
        ui.hotspot[t] = o;
        lv_obj_set_style_bg_opa(o, LV_OPA_TRANSP, 0);
        lv_obj_set_style_bg_opa(o, LV_OPA_20, LV_STATE_PRESSED);
        lv_obj_t *disc = box(o, f->badge_x - f->x - 11, f->badge_y - f->y - 11, 23, 23, PAPER, 12);
        image(disc, pw_icons[f->icon], 0, 0);
        /* Furniture takes precedence over the cat, with the modal above both. */
        lv_obj_move_to_index(o, lv_obj_get_index(ui.door));
    }
}
static void build_panel(lv_obj_t *screen)
{
    ui.layer = box(screen, 0, 0, 390, 450, PAPER, 0);
    lv_obj_set_style_bg_opa(ui.layer, LV_OPA_TRANSP, 0);
    lv_obj_t *scrim = button(ui.layer, 0, 0, 390, 450, TAG_CLOSE, 0x223A46, 0);
    lv_obj_set_style_bg_opa(scrim, LV_OPA_50, 0);
    ui.dialog = box(ui.layer, 57, 95, 276, 260, 0xF9FCFD, 18);
    ui.title = text(ui.dialog, "小猫状态", 18, 13, &pet_font_20, INK);
    lv_obj_t *close = button(ui.dialog, 226, 2, 44, 44, TAG_CLOSE, 0xF9FCFD, 12);
    lv_obj_t *x = text(close, "×", 0, 0, &pet_font_20, BLUE);
    lv_obj_center(x);
    box(ui.dialog, 18, 48, 240, 1, 0xD4E0E5, 0);
    ui.status = box(ui.dialog, 18, 57, 240, 194, 0xF9FCFD, 0);
    text(ui.status, "饱食", 0, 0, &pet_font_16, INK);
    ui.food = text(ui.status, "70", 194, 0, &pet_font_20, INK);
    lv_obj_set_width(ui.food, 46);
    lv_obj_set_style_text_align(ui.food, LV_TEXT_ALIGN_RIGHT, 0);
    box(ui.status, 0, 33, 240, 7, 0xE1E9ED, 3);
    ui.food_fill = box(ui.status, 0, 33, 168, 7, 0xBF9769, 3);
    text(ui.status, "心情", 0, 52, &pet_font_16, INK);
    ui.mood = text(ui.status, "60", 194, 52, &pet_font_20, INK);
    lv_obj_set_width(ui.mood, 46);
    lv_obj_set_style_text_align(ui.mood, LV_TEXT_ALIGN_RIGHT, 0);
    box(ui.status, 0, 85, 240, 7, 0xE1E9ED, 3);
    ui.mood_fill = box(ui.status, 0, 85, 144, 7, 0x88AA92, 3);
    ui.state = text(ui.status, "闲逛中", 0, 108, &pet_font_16, BLUE);
    text(ui.status, "v" PET_FIRMWARE_VERSION " · 自动存档", 0, 139, &pet_font_16, INK);
    ui.save_note = text(ui.status, save_notes[save_state], 0, 166, &pet_font_16, 0x62736B);
    ui.travel = box(ui.dialog, 18, 57, 240, 194, 0xF9FCFD, 0);
    text(ui.travel, "鸿雁路西 · 雁北园", 8, 6, &pet_font_20, INK);
    text(ui.travel, "鸿雁路东 · 教综楼", 8, 49, &pet_font_20, INK);
    text(ui.travel, "国脉路南 · 学生餐厅", 8, 92, &pet_font_20, INK);
    text(ui.travel, "点路面走，点楼门进入", 8, 145, &pet_font_16, BLUE);
    visible(ui.layer, false);
}
void pet_ui_create(pet_ui_action_fn callback)
{
    /* No UI-owned model or animation timer: recreation cannot duplicate settlement. */
    lv_obj_t *screen = lv_screen_active();
    lv_obj_clean(screen);
    memset(&ui, 0, sizeof(ui));
    ui.action = callback;
    ui.scene = -1;
    lv_obj_remove_flag(screen, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(screen, lv_color_hex(PAPER), 0);
    ui.background = lv_image_create(screen);
    lv_obj_add_flag(ui.background, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(ui.background, ground_click, LV_EVENT_CLICKED, NULL);
    lv_obj_set_pos(ui.background, 0, 0);
    lv_image_set_src(ui.background, room_art[0]);
    build_campus(screen);
    ui.shadow = box(screen, 0, 0, 32, 8, 0x60523D, LV_RADIUS_CIRCLE);
    lv_obj_set_style_bg_opa(ui.shadow, LV_OPA_20, 0);
    ui.sprite = lv_image_create(screen);
    lv_obj_remove_flag(ui.sprite, LV_OBJ_FLAG_CLICKABLE);
    lv_image_set_src(ui.sprite, pw_frames[0]);
    lv_image_set_pivot(ui.sprite, 36, 64);
    ui.cat_hit = button(screen, 0, 0, 64, 68, PW_TOUCH, PAPER, 0);
    lv_obj_set_style_bg_opa(ui.cat_hit, LV_OPA_TRANSP, 0);
    lv_obj_set_style_bg_opa(ui.cat_hit, LV_OPA_TRANSP, LV_STATE_PRESSED);
    ui.foreground = lv_image_create(screen);
    lv_obj_remove_flag(ui.foreground, LV_OBJ_FLAG_CLICKABLE);
    lv_image_set_src(ui.foreground, &shahe_front_0);
    ui.door = button(screen, 310, 100, 69, 143, PW_EXIT, PAPER, 5);
    lv_obj_set_style_bg_opa(ui.door, LV_OPA_TRANSP, 0);
    lv_obj_set_style_bg_opa(ui.door, LV_OPA_TRANSP, LV_STATE_PRESSED);
    gate_badge(ui.door, "出门", 1, 65, 63);
    build_gates(screen);
    ui.location = box(screen, 13, 13, 206, 52, PAPER, 14);
    lv_obj_set_style_bg_opa(ui.location, LV_OPA_90, 0);
    text(ui.location, "沙河校园小猫", 12, 2, &pet_font_16, BLUE);
    ui.location_text = text(ui.location, room_names[0], 12, 22, &pet_font_20, INK);
    ui.marks = text(screen, "", 0, 0, &pet_font_16, BLUE);
    ui.bubble = box(screen, 67, 398, 256, 36, PAPER, 12);
    lv_obj_set_style_bg_opa(ui.bubble, LV_OPA_90, 0);
    ui.bubble_text = text(ui.bubble, "", 6, 6, &pet_font_16, INK);
    lv_obj_set_width(ui.bubble_text, 244);
    lv_obj_set_style_text_align(ui.bubble_text, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_long_mode(ui.bubble_text, LV_LABEL_LONG_CLIP);
    build_panel(screen);
    for (int i = 0; i < 2; i++)
    {
        ui.corner[i] = button(screen, 265 + i * 60, 13, 52, 52, i ? TAG_TRAVEL : TAG_STATUS, PAPER, 14);
        lv_obj_set_style_bg_opa(ui.corner[i], LV_OPA_90, 0);
        image(ui.corner[i], pw_icons[i], 14, 4);
        text(ui.corner[i], i ? "路标" : "状态", 10, 29, &pet_font_16, BLUE);
    }
}
static int limit(int x, int lo, int hi)
{
    return x < lo ? lo : (x > hi ? hi : x);
}
static void paint(void)
{
    if (ui.scene != ui.world.scene)
    {
        ui.scene = ui.world.scene;
        build_hotspots();
        if (ui.scene != PW_CAMPUS)
            lv_image_set_src(ui.background, room_art[ui.scene]);
    }
    bool campus = ui.world.scene == PW_CAMPUS;
    visible(ui.campus, campus);
    visible(ui.background, !campus);
    visible(ui.door, !campus);
    visible(ui.corner[1], campus);
    if (!campus)
        lv_label_set_text(ui.location_text, room_names[ui.scene]);
    float cx, cy;
    pw_camera(&ui.world, &cx, &cy);
    int left = (int)lroundf(cx), top = (int)lroundf(cy);
    lv_obj_set_pos(ui.campus, -left, -top);
    lv_obj_set_pos(ui.gates, -left, -top);
    visible(ui.gates, campus);
    if (campus)
        lv_label_set_text(ui.location_text, ui.world.y > 475 ? "国脉路南段" : "鸿雁路一带");
    visible(ui.goal, campus && ui.world.phase == PW_WALK && ui.world.purpose == 3);
    lv_obj_set_pos(ui.goal, (int)ui.world.point_goal.x - 10, (int)ui.world.point_goal.y - 10);
    pw_visual_t v;
    pw_visual(&ui.world, &v);
    v.x -= left;
    v.y -= top;
    lv_image_set_src(ui.sprite, pw_frames[v.frame]);
    lv_obj_set_pos(ui.sprite, (int)lroundf(v.x) - 36, (int)lroundf(v.y) - 64);
    lv_image_set_scale(ui.sprite, (uint32_t)lroundf(v.scale * 256));
    lv_obj_set_pos(ui.cat_hit, limit((int)v.x - 32, 0, 326), limit((int)v.y - 64, 0, 382));
    int rx = v.sleeping ? 22 : 12;
    lv_obj_set_size(ui.shadow, rx * 2, v.sleeping ? 12 : 6);
    lv_obj_set_pos(ui.shadow, (int)v.x - rx, (int)v.y + (v.sleeping ? -16 : -2));
    lv_obj_set_style_bg_opa(ui.shadow, v.airborne ? LV_OPA_0 : LV_OPA_20, 0);
    bool bed = ui.world.phase == PW_SLEEP || ui.world.action == PW_REST || ui.world.action == PW_WAKE;
    bool show_front = !campus && (v.occlusion || (bed && v.y < 170));
    if (show_front)
    {
        static const lv_image_dsc_t *const front[] = {&shahe_seat_0, &shahe_front_1, &shahe_front_2};
        static const int16_t *const anchor[] = {shahe_seat_0_xy, shahe_front_1_xy, shahe_front_2_xy};
        const int16_t *xy = bed ? shahe_front_0_xy : anchor[ui.scene];
        lv_image_set_src(ui.foreground, bed ? &shahe_front_0 : front[ui.scene]);
        lv_obj_set_pos(ui.foreground, xy[0], xy[1]);
    }
    visible(ui.foreground, show_front);
    lv_label_set_text(ui.marks,
                      v.sleeping ? "z z"
                                 : (ui.world.phase == PW_ACTION && ui.world.action == PW_TOUCH ? "♥" : ""));
    lv_obj_set_pos(ui.marks, limit((int)v.x + 18, 0, 350), limit((int)v.y - 51, 0, 420));
    lv_label_set_text_fmt(ui.food, "%u", ui.world.food);
    lv_label_set_text_fmt(ui.mood, "%u", ui.world.mood);
    lv_obj_set_width(ui.food_fill, ui.world.food ? ui.world.food * 240 / 100 : 1);
    lv_obj_set_width(ui.mood_fill, ui.world.mood ? ui.world.mood * 240 / 100 : 1);
    visible(ui.food_fill, ui.world.food > 0);
    visible(ui.mood_fill, ui.world.mood > 0);
    lv_label_set_text(ui.state, pw_state_text(&ui.world));
    visible(ui.layer, ui.panel != 0);
    visible(ui.status, ui.panel == 1);
    visible(ui.travel, ui.panel == 2);
    lv_label_set_text(ui.title, ui.panel == 2 ? "校园路标" : "小猫状态");
    for (int i = 0; i < 2; i++)
        lv_obj_set_style_bg_color(ui.corner[i], lv_color_hex(ui.panel == i + 1 ? 0xDCEAF3 : PAPER), 0);
    const char *msg = pw_message_text(&ui.world);
    if (ui.world.now < 2300 && !*msg)
        msg = "v" PET_FIRMWARE_VERSION " · 沙河校园";
    if (ui.world.phase == PW_ACTION && ui.world.action == PW_EAT && ui.world.now < ui.world.message_until)
        lv_label_set_text_fmt(ui.bubble_text, "开饭啦，饱食 +%d", ui.world.gain);
    else
        lv_label_set_text(ui.bubble_text, msg);
    visible(ui.bubble, *msg != 0);
}
void pet_ui_update(const pet_world_t *world)
{
    ui.world = *world;
    paint();
}
