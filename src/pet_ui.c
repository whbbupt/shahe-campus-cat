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
    TAG_CLOSE,
    TAG_PHONE,
    TAG_JOURNAL,
    TAG_MEMORY_CLOSE
};
enum { PANEL_NONE, PANEL_STATUS, PANEL_ORDER, PANEL_JOURNAL };
static struct
{
    pet_ui_action_fn action;
    pet_ui_point_fn point;
    lv_obj_t *campus, *gates, *door, *location, *location_text, *goal;
    pet_world_t world;
    int scene;
    unsigned panel;
    lv_obj_t *background, *shadow, *sprite, *cat_hit, *foreground, *marks, *hotspot[2];
    lv_obj_t *layer, *food, *mood, *food_fill, *mood_fill, *state, *save_note;
    lv_obj_t *status_button, *bubble, *bubble_text, *decor, *phone, *bag;
    lv_obj_t *audio_label, *audio_note;
    lv_obj_t *status_dialog, *order_dialog, *order_note, *order_save, *order_button;
    lv_obj_t *journal_button, *journal_dialog, *journal_page, *wish[3], *wish_mark[3],
             *journal_count, *claim, *claim_hint, *badge[3];
    lv_obj_t *blackboard, *memory_dialog, *memory_note, *memory_round, *memory_tile[4], *memory_footer;
} ui;
static bool audio_available = true;
static unsigned save_state = PET_SAVE_PENDING;
static const char *save_notes[] = {"等待自动保存", "已保存，重启可恢复", "存档暂不可用", "保存失败，稍后重试"};
void pet_ui_set_save_status(unsigned status)
{
    save_state = status <= PET_SAVE_FAILED ? status : PET_SAVE_UNAVAILABLE;
    if (ui.save_note) lv_label_set_text(ui.save_note, save_notes[save_state]);
}
static void paint(void);
void pet_ui_set_audio_available(bool available)
{
    audio_available = available;
}
void pet_ui_set_point_handler(pet_ui_point_fn handler)
{
    ui.point = handler;
}
static void ground_click(lv_event_t *e)
{
    (void)e;
    if (ui.panel || ui.world.phase == PW_MINIGAME || !ui.point)
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
    if (cmd == TAG_MEMORY_CLOSE || (cmd == TAG_CLOSE && ui.world.phase == PW_MINIGAME))
    {
        if (ui.action) ui.action(PW_MEMORY_CANCEL);
        return;
    }
    if (ui.world.phase == PW_MINIGAME)
    {
        if (cmd >= PW_MEMORY_0 && cmd <= PW_MEMORY_3 && ui.action)
            ui.action((pw_command_t)cmd);
        return;
    }
    if (cmd == TAG_JOURNAL)
    {
        ui.panel = ui.panel == PANEL_JOURNAL ? PANEL_NONE : PANEL_JOURNAL;
        paint();
        return;
    }
    if (cmd == PW_JOURNAL_CLAIM && ui.panel == PANEL_JOURNAL)
    {
        if (ui.action) ui.action(PW_JOURNAL_CLAIM);
        return;
    }
    if (cmd == PW_SOUND_NEXT && ui.panel == PANEL_STATUS)
    {
        if (ui.action) ui.action(PW_SOUND_NEXT);
        return;
    }
    if (cmd == TAG_STATUS)
    {
        ui.panel = ui.panel == PANEL_STATUS ? PANEL_NONE : PANEL_STATUS;
        paint();
        return;
    }
    if (cmd == TAG_CLOSE)
    {
        ui.panel = PANEL_NONE;
        paint();
        return;
    }
    if (cmd == TAG_PHONE)
    {
        ui.panel = PANEL_ORDER;
        paint();
        return;
    }
    if (cmd == PW_ORDER && ui.panel == PANEL_ORDER)
    {
        if (ui.action && ui.action(PW_ORDER) == PW_OK)
        {
            ui.panel = PANEL_NONE;
            paint();
        }
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
static const char *const campus_names[] = {"雁北园", "教综楼", "学生餐厅", "健身房"};
static const pw_scene_t campus_rooms[] = {PW_DORM, PW_CLASSROOM, PW_CANTEEN, PW_GYM};

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
    for (unsigned y = 0; y < PW_CAMPUS_HEIGHT / 260; y++)
        for (unsigned x = 0; x < PW_CAMPUS_WIDTH / 280; x++)
            image(ui.campus, shahe_ground[y * (PW_CAMPUS_WIDTH / 280) + x], x * 280, y * 260);
    static const int16_t trees[][2] = {
        {65,136}, {146,111}, {245,142}, {105,249}, {249,250},
        {590,121}, {691,117}, {875,112}, {996,123}, {1090,150},
        {590,321}, {688,338}, {885,318}, {988,327}, {1085,304},
        {116,443}, {93,548}, {279,388}, {282,621}, {450,373},
        {583,444}, {603,562}, {816,459}, {1102,555}, {1242,475},
        {107,729}, {288,729}, {444,727}, {696,745}, {816,778},
        {1087,859}, {1273,856}, {418,948}, {574,951}, {894,965},
        {1001,970}, {1262,971}, {253,1096}, {445,1076}, {710,1084},
        {892,1093}, {1035,1092}, {1244,1110}};
    for (unsigned i = 0; i < sizeof(trees) / sizeof(trees[0]); i++)
    {
        int x = trees[i][0], y = trees[i][1];
        int h = shahe_tree.header.h;
        image(ui.campus, &shahe_tree_shadow, x + 18, y + h - 12);
        image(ui.campus, &shahe_tree, x, y);
    }
    image(ui.campus, &shahe_library, 868, 472);
    image(ui.campus, &shahe_center, 690, 566);
    image(ui.campus, &shahe_lecture, 617, 751);
    image(ui.campus, &shahe_lecture, 1215, 750);
    image(ui.campus, &shahe_lecture, 1213, 1084);
    image(ui.campus, &shahe_building_0, 401, 792);
    image(ui.campus, &shahe_gate, 12, 629);
    image(ui.campus, &shahe_gate, 328, 1175);
    image(ui.campus, &shahe_gate, 743, 1175);
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
    ui.goal = box(ui.campus, 0, 0, 20, 20, BLUE, LV_RADIUS_CIRCLE);
    lv_obj_set_style_bg_opa(ui.goal, LV_OPA_20, 0);
    lv_obj_set_style_border_width(ui.goal, 2, 0);
    lv_obj_set_style_border_color(ui.goal, lv_color_hex(BLUE), 0);
}

static void build_gates(lv_obj_t *screen)
{
    ui.gates = box(screen, 0, 0, PW_CAMPUS_WIDTH, PW_CAMPUS_HEIGHT, PAPER, 0);
    lv_obj_set_style_bg_opa(ui.gates, LV_OPA_TRANSP, 0);
    for (unsigned room = 0; room < sizeof(campus_rooms) / sizeof(campus_rooms[0]); room++)
    {
        pw_point_t door = pw_node(PW_CAMPUS, pw_campus_door(campus_rooms[room]));
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

static const furniture_hotspot_t furniture[PW_GYM + 1][2] = {
    {{24, 110, 134, 61, 110, 122, PW_REST, 2}, {31, 175, 87, 80, 92, 208, PW_CODE, 3}},
    {{44, 235, 90, 75, 111, 245, PW_STUDY, 4}},
    {{36, 231, 105, 83, 117, 243, PW_EAT, 5}},
    [PW_GYM] = {{38, 166, 126, 152, 132, 180, PW_WORKOUT, 6}}
};

static const lv_image_dsc_t *const room_art[] = {
    &shahe_room_0, &shahe_room_1, &shahe_room_2, [PW_GYM] = &shahe_room_3};
static const char *const room_names[] = {
    "雁北园 · 宿舍", "教综楼 · 教室", "学生餐厅 · 食堂", [PW_GYM] = "活动中心 · 健身房"};
static const lv_image_dsc_t *const room_decor[] = {
    &shahe_decor_0, &shahe_decor_1, &shahe_decor_2, [PW_GYM] = &shahe_decor_3};
static const int decor_anchor[][2] = {{16,407}, {314,383}, {319,371}, [PW_GYM] = {289,369}};

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
        if (f->command == PW_WORKOUT)
        {
            box(disc, 4, 15, 15, 3, BLUE, 1);
            box(disc, 14, 6, 2, 10, BLUE, 1);
            box(disc, 13, 5, 6, 3, BLUE, 1);
        }
        else
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
    lv_obj_t *dialog = box(ui.layer, 57, 50, 276, 350, 0xF9FCFD, 18);
    ui.status_dialog = dialog;
    text(dialog, "小猫状态", 18, 13, &pet_font_20, INK);
    lv_obj_t *close = button(dialog, 226, 2, 44, 44, TAG_CLOSE, 0xF9FCFD, 12);
    lv_obj_t *x = text(close, "×", 0, 0, &pet_font_20, BLUE);
    lv_obj_center(x);
    box(dialog, 18, 48, 240, 1, 0xD4E0E5, 0);
    lv_obj_t *status = box(dialog, 18, 57, 240, 194, 0xF9FCFD, 0);
    text(status, "饱食", 0, 0, &pet_font_16, INK);
    ui.food = text(status, "70", 194, 0, &pet_font_20, INK);
    lv_obj_set_width(ui.food, 46);
    lv_obj_set_style_text_align(ui.food, LV_TEXT_ALIGN_RIGHT, 0);
    box(status, 0, 33, 240, 7, 0xE1E9ED, 3);
    ui.food_fill = box(status, 0, 33, 168, 7, 0xBF9769, 3);
    text(status, "心情", 0, 52, &pet_font_16, INK);
    ui.mood = text(status, "60", 194, 52, &pet_font_20, INK);
    lv_obj_set_width(ui.mood, 46);
    lv_obj_set_style_text_align(ui.mood, LV_TEXT_ALIGN_RIGHT, 0);
    box(status, 0, 85, 240, 7, 0xE1E9ED, 3);
    ui.mood_fill = box(status, 0, 85, 144, 7, 0x88AA92, 3);
    ui.state = text(status, "闲逛中", 0, 108, &pet_font_16, BLUE);
    text(status, "v" PET_FIRMWARE_VERSION " · 自动存档", 0, 139, &pet_font_16, INK);
    ui.save_note = text(status, save_notes[save_state], 0, 166, &pet_font_16, 0x62736B);
    lv_obj_t *sound = button(dialog, 18, 255, 240, 44, PW_SOUND_NEXT, 0xDCE6CB, 10);
    ui.audio_label = text(sound, "", 0, 0, &pet_font_16, INK);
    lv_obj_center(ui.audio_label);
    ui.audio_note = text(dialog, "", 18, 307, &pet_font_16, BLUE);
    ui.order_dialog = box(ui.layer, 57, 113, 276, 224, PAPER, 18);
    text(ui.order_dialog, "宿舍外卖", 18, 13, &pet_font_20, INK);
    lv_obj_t *order_close = button(ui.order_dialog, 226, 2, 44, 44, TAG_CLOSE, PAPER, 12);
    lv_obj_t *order_x = text(order_close, "×", 0, 0, &pet_font_20, BLUE);
    lv_obj_center(order_x);
    box(ui.order_dialog, 18, 51, 240, 1, 0xD8CFB8, 0);
    text(ui.order_dialog, "暖饭便当", 18, 62, &pet_font_20, INK);
    text(ui.order_dialog, "送到宿舍门旁 · 饱食 +12", 18, 93, &pet_font_16, BLUE);
    ui.order_note = text(ui.order_dialog, "", 18, 125, &pet_font_16, INK);
    ui.order_save = text(ui.order_dialog, "", 18, 163, &pet_font_16, BLUE);
    ui.order_button = button(ui.order_dialog, 18, 163, 240, 44, PW_ORDER, 0xDCE6CB, 10);
    lv_obj_t *order = text(ui.order_button, "确认下单", 0, 0, &pet_font_16, INK);
    lv_obj_center(order);
    ui.journal_dialog = box(ui.layer, 36, 76, 318, 308, PAPER, 18);
    text(ui.journal_dialog, "小猫的校园手账", 18, 12, &pet_font_20, INK);
    lv_obj_t *journal_close = button(ui.journal_dialog, 267, 2, 44, 44, TAG_CLOSE, PAPER, 12);
    lv_obj_t *journal_x = text(journal_close, "×", 0, 0, &pet_font_20, BLUE);
    lv_obj_center(journal_x);
    ui.journal_page = text(ui.journal_dialog, "", 18, 42, &pet_font_16, BLUE);
    for (unsigned i = 0; i < 3; i++)
    {
        lv_obj_t *row = box(ui.journal_dialog, 18, 74 + i * 41, 282, 35, 0xF0EAD8, 9);
        ui.wish_mark[i] = text(row, "", 10, 5, &pet_font_16, BLUE);
        ui.wish[i] = text(row, "", 39, 5, &pet_font_16, INK);
        ui.badge[i] = box(ui.journal_dialog, 18 + i * 37, 207, 28, 28, 0xDCD9C8, 14);
        lv_obj_t *star = text(ui.badge[i], "★", 0, 0, &pet_font_16, INK);
        lv_obj_center(star);
    }
    ui.journal_count = text(ui.journal_dialog, "", 137, 210, &pet_font_16, BLUE);
    ui.claim_hint = text(ui.journal_dialog, "", 18, 250, &pet_font_16, BLUE);
    ui.claim = button(ui.journal_dialog, 18, 248, 282, 44, PW_JOURNAL_CLAIM, 0xDCE6CB, 10);
    lv_obj_t *claim_name = text(ui.claim, "收下贴纸，翻到下一页", 0, 0, &pet_font_16, INK);
    lv_obj_center(claim_name);

    ui.memory_dialog = box(ui.layer, 29, 75, 332, 312, PAPER, 18);
    text(ui.memory_dialog, "课堂记忆题", 18, 12, &pet_font_20, INK);
    lv_obj_t *memory_close = button(ui.memory_dialog, 282, 2, 44, 44, TAG_MEMORY_CLOSE, PAPER, 12);
    lv_obj_t *memory_x = text(memory_close, "×", 0, 0, &pet_font_20, BLUE);
    lv_obj_center(memory_x);
    ui.memory_note = text(ui.memory_dialog, "", 18, 50, &pet_font_20, INK);
    ui.memory_round = text(ui.memory_dialog, "", 18, 80, &pet_font_16, BLUE);
    static const char *symbols[] = {"花", "叶", "月", "星"};
    for (unsigned i = 0; i < 4; i++)
    {
        ui.memory_tile[i] = button(ui.memory_dialog, 20 + (i % 2) * 152,
                                   111 + (i / 2) * 83, 140, 72, PW_MEMORY_0 + i, 0xE7E4CF, 13);
        lv_obj_t *symbol = text(ui.memory_tile[i], symbols[i], 0, 0, &pet_font_20, INK);
        lv_obj_center(symbol);
    }
    ui.memory_footer = text(ui.memory_dialog, "", 20, 281, &pet_font_16, BLUE);
    visible(ui.layer, false);
}
static void build_dorm_items(lv_obj_t *screen)
{
    ui.phone = button(screen, 118, 191, 44, 61, TAG_PHONE, PAPER, 7);
    lv_obj_set_style_bg_opa(ui.phone, LV_OPA_TRANSP, 0);
    box(ui.phone, 13, 9, 17, 25, 0x34596A, 4);
    box(ui.phone, 15, 11, 13, 19, 0xA4C1C0, 3);
    box(ui.phone, 19, 31, 5, 1, PAPER, 1);
    lv_obj_t *phone_tag = box(ui.phone, 2, 34, 40, 25, PAPER, 6);
    text(phone_tag, "外卖", 4, 0, &pet_font_16, INK);

    ui.bag = button(screen, 278, 248, 52, 51, PW_TAKEOUT, PAPER, 7);
    lv_obj_set_style_bg_opa(ui.bag, LV_OPA_TRANSP, 0);
    box(ui.bag, 19, 1, 14, 13, 0xA77952, 5);
    box(ui.bag, 22, 4, 8, 8, PAPER, 3);
    box(ui.bag, 10, 9, 32, 25, 0xC58D5D, 4);
    box(ui.bag, 13, 10, 24, 20, 0xE7B980, 3);
    lv_obj_t *bag_tag = box(ui.bag, 5, 26, 42, 24, PAPER, 6);
    text(bag_tag, "取餐", 5, 0, &pet_font_16, INK);
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
    ui.decor = lv_image_create(screen);
    lv_obj_remove_flag(ui.decor, LV_OBJ_FLAG_CLICKABLE);
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
    build_dorm_items(screen);
    ui.door = button(screen, 310, 100, 69, 143, PW_EXIT, PAPER, 5);
    lv_obj_set_style_bg_opa(ui.door, LV_OPA_TRANSP, 0);
    lv_obj_set_style_bg_opa(ui.door, LV_OPA_TRANSP, LV_STATE_PRESSED);
    gate_badge(ui.door, "出门", 1, 65, 63);
    build_gates(screen);
    ui.blackboard = button(screen, 147, 108, 145, 71, PW_MEMORY_START, PAPER, 7);
    lv_obj_set_style_bg_opa(ui.blackboard, LV_OPA_TRANSP, 0);
    gate_badge(ui.blackboard, "记忆题", 59, 40, 80);
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
    ui.journal_button = button(screen, 265, 13, 52, 52, TAG_JOURNAL, PAPER, 14);
    lv_obj_set_style_bg_opa(ui.journal_button, LV_OPA_90, 0);
    image(ui.journal_button, pw_icons[4], 14, 4);
    text(ui.journal_button, "手账", 10, 29, &pet_font_16, BLUE);
    ui.status_button = button(screen, 325, 13, 52, 52, TAG_STATUS, PAPER, 14);
    lv_obj_set_style_bg_opa(ui.status_button, LV_OPA_90, 0);
    image(ui.status_button, pw_icons[0], 14, 4);
    text(ui.status_button, "状态", 10, 29, &pet_font_16, BLUE);
}
static int limit(int x, int lo, int hi)
{
    return x < lo ? lo : (x > hi ? hi : x);
}
static const char *campus_area(float x, float y)
{
    if (y < 395)
        return "校园北区";
    if (x < 350 && y < 665)
        return "运动场一带";
    if (x < 220 && y < 780)
        return "西门一带";
    if (x < 475 && y > 1140)
        return "西南门一带";
    if (x > 704 && x < 895 && y > 1160)
        return "南门一带";
    if (x > 825 && x < 1160 && y < 642)
        return "图书馆一带";
    if (x > 605 && x < 794 && y < 665)
        return "学生活动中心";
    if (x > 1185 && y > 690)
        return "东区教学区";
    return y > 775 ? "国脉路南段" : "鸿雁路一带";
}

static void paint_order(void)
{
    const pet_world_t *w = &ui.world;
    bool can_order = w->scene == PW_DORM && w->order == PW_ORDER_NONE && w->cooldown[PW_TAKEOUT] <= w->now && !pw_busy(w);
    if (w->order == PW_ORDER_DELIVERING)
        lv_label_set_text_fmt(ui.order_note, "配送中 · 约 %u 秒", (unsigned)((w->order_due - w->now + 999) / 1000));
    else if (w->order == PW_ORDER_READY)
        lv_label_set_text(ui.order_note, "已送到 · 点门旁纸袋");
    else if (w->cooldown[PW_TAKEOUT] > w->now)
        lv_label_set_text_fmt(ui.order_note, "约 %u 秒后可再下单",
                              (unsigned)((w->cooldown[PW_TAKEOUT] - w->now + 999) / 1000));
    else if (pw_busy(w))
        lv_label_set_text(ui.order_note, w->phase == PW_SLEEP ? "先点床叫醒小猫" : "等当前动作结束");
    else
        lv_label_set_text(ui.order_note, "约 30 秒送达");
    lv_label_set_text(ui.order_save, w->order == PW_ORDER_NONE ? "" : save_notes[save_state]);
    visible(ui.order_save, !can_order);
    visible(ui.order_button, can_order);
}

static void paint_journal(void)
{
    const pet_world_t *w = &ui.world;
    lv_label_set_text_fmt(ui.journal_page, "第 %u 页 · 三件小愿望", (unsigned)w->journal_page);
    for (unsigned i = 0; i < 3; i++)
    {
        bool done = (w->journal_progress & (1U << i)) != 0;
        lv_label_set_text(ui.wish[i], pw_wish_text(pw_journal_wish(w, i)));
        lv_label_set_text(ui.wish_mark[i], done ? "✓" : "○");
        lv_obj_set_style_bg_color(ui.badge[i], lv_color_hex(w->journal_badges & (1U << i) ? 0xD5DDA8 : 0xE0DDCE), 0);
    }
    lv_label_set_text_fmt(ui.journal_count, "贴纸 %u 枚", w->journal_stamps);
    bool ready = w->journal_progress == 7 && !pw_busy(w);
    visible(ui.claim, ready);
    visible(ui.claim_hint, !ready);
    lv_label_set_text(ui.claim_hint, w->journal_progress != 7 ? "按自己的顺序慢慢完成"
                        : w->phase == PW_SLEEP ? "点床叫醒小猫后收贴纸" : "等当前活动结束再收贴纸");
}
static void paint_memory(void)
{
    const pm_game_t *game = &ui.world.memory;
    const char *note = game->phase == PM_SHOW ? "先记住亮起的顺序"
                       : game->phase == PM_INPUT ? "按刚才的顺序点"
                       : game->phase == PM_WON ? "都记住啦！" : "差一点，下次再试";
    lv_label_set_text(ui.memory_note, note);
    lv_label_set_text_fmt(ui.memory_round, "第 %u / 3 轮 · %u 个图案", game->round + 1, game->length);
    int highlight = pm_highlight(game);
    static const uint32_t colors[] = {0xE4B9A1, 0xBFCEA5, 0xB5CDD2, 0xE5D49D};
    for (unsigned i = 0; i < 4; i++)
    {
        lv_obj_set_style_bg_color(ui.memory_tile[i], lv_color_hex(highlight == (int)i ? colors[i] : 0xE8E3D2), 0);
        lv_obj_set_style_border_width(ui.memory_tile[i], highlight == (int)i ? 3 : 0, 0);
        lv_obj_set_style_border_color(ui.memory_tile[i], lv_color_hex(BLUE), 0);
    }
    if (game->phase == PM_INPUT)
        lv_label_set_text_fmt(ui.memory_footer, "已经想起 %u / %u 个", game->input_at, game->length);
    else if (game->phase == PM_WON)
        lv_label_set_text_fmt(ui.memory_footer, "心情 +%d · 课堂记忆已记下", ui.world.mood_gain);
    else
        lv_label_set_text(ui.memory_footer, game->phase == PM_SHOW ? "看完再点，不用赶时间"
                                                                  : "随时可以重新挑战");
}

static void paint(void)
{
    if (ui.scene != ui.world.scene)
    {
        ui.scene = ui.world.scene;
        if (ui.scene != PW_DORM && ui.panel == PANEL_ORDER)
            ui.panel = PANEL_NONE;
        build_hotspots();
        if (ui.scene != PW_CAMPUS)
        {
            lv_image_set_src(ui.background, room_art[ui.scene]);
            const lv_image_dsc_t *art = room_decor[ui.scene];
            lv_image_set_src(ui.decor, art);
            lv_obj_set_pos(ui.decor, decor_anchor[ui.scene][0], decor_anchor[ui.scene][1] - art->header.h);
        }
    }
    bool campus = ui.world.scene == PW_CAMPUS;
    visible(ui.campus, campus);
    visible(ui.background, !campus);
    visible(ui.decor, !campus);
    visible(ui.phone, ui.world.scene == PW_DORM);
    visible(ui.blackboard, ui.world.scene == PW_CLASSROOM);
    visible(ui.status_button, ui.world.phase != PW_MINIGAME);
    visible(ui.journal_button, ui.world.phase != PW_MINIGAME);
    visible(ui.bag, ui.world.scene == PW_DORM && ui.world.order == PW_ORDER_READY &&
                    ui.world.action != PW_TAKEOUT);
    visible(ui.door, !campus);
    if (!campus)
        lv_label_set_text(ui.location_text, room_names[ui.scene]);
    float cx, cy;
    pw_camera(&ui.world, &cx, &cy);
    int left = (int)lroundf(cx), top = (int)lroundf(cy);
    lv_obj_set_pos(ui.campus, -left, -top);
    lv_obj_set_pos(ui.gates, -left, -top);
    visible(ui.gates, campus);
    if (campus)
        lv_label_set_text(ui.location_text, campus_area(ui.world.x, ui.world.y));
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
    static const char *modes[] = {"声音：静音", "声音：仅音效", "声音：音乐＋音效"};
    lv_label_set_text(ui.audio_label, modes[ui.world.sound_mode <= 2 ? ui.world.sound_mode : 0]);
    lv_obj_center(ui.audio_label);
    lv_label_set_text(ui.audio_note, audio_available ? "点按切换 · 自动记住" : "声音暂不可用，切换重试");
    bool memory = ui.world.phase == PW_MINIGAME;
    visible(ui.layer, memory || ui.panel != PANEL_NONE);
    visible(ui.status_dialog, !memory && ui.panel == PANEL_STATUS);
    visible(ui.order_dialog, !memory && ui.panel == PANEL_ORDER);
    visible(ui.journal_dialog, !memory && ui.panel == PANEL_JOURNAL);
    visible(ui.memory_dialog, memory);
    if (memory) paint_memory();
    if (ui.panel == PANEL_JOURNAL) paint_journal();
    if (ui.panel == PANEL_ORDER)
        paint_order();
    lv_obj_set_style_bg_color(ui.status_button, lv_color_hex(ui.panel == PANEL_STATUS ? 0xDCEAF3 : PAPER), 0);
    const char *msg = pw_message_text(&ui.world);
    if (ui.world.now < 2300 && !*msg)
        msg = "v" PET_FIRMWARE_VERSION " · 沙河校园";
    bool gains = ui.world.now < ui.world.gain_until;
    if (gains && ui.world.gain && ui.world.mood_gain)
        lv_label_set_text_fmt(ui.bubble_text, "饱食 %+d · 心情 %+d", ui.world.gain, ui.world.mood_gain);
    else if (gains && ui.world.gain)
        lv_label_set_text_fmt(ui.bubble_text, "饱食 %+d", ui.world.gain);
    else if (gains)
        lv_label_set_text_fmt(ui.bubble_text, "心情 %+d", ui.world.mood_gain);
    else
        lv_label_set_text(ui.bubble_text, msg);
    visible(ui.bubble, gains || *msg);
}
void pet_ui_update(const pet_world_t *world)
{
    ui.world = *world;
    paint();
}
