/* Room views and continuous campus camera. Model owns movement and settlement. */
#include "pet_ui.h"
#include "lvgl.h"
#include "pet_version.h"
#include "pet_world_assets.h"
#include "pet_shahe_assets.h"
#include "pet_world_font.h"
#include "pet_food_assets.h"
#include "pet_furniture_assets.h"
#include <math.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>

#define INK 0x304C4A
#define BLUE 0x456B77
#define PAPER 0xF8F3DF
#define HUD_PAPER 0xFAEBCF
#define HUD_INK 0x573F30
#define HUD_EDGE 0xAE8766
#define HUD_BRASS 0xD9AF63
enum
{
    TAG_STATUS = 300,
    TAG_CLOSE,
    TAG_PHONE,
    TAG_SHOP,
    TAG_MEMORY_CLOSE,
    TAG_SHOP_FOOD,
    TAG_SHOP_DECOR,
    TAG_CONFIRM,
    TAG_ALBUM,
    TAG_ACTIVITY,
    TAG_WINDOW_FIRST,
    TAG_LIBRARY = 330,
    TAG_PUBLIC,
    TAG_LAB,
    TAG_WISDOM,
    TAG_SOUTH,
    TAG_FLAVOR,
    TAG_STAFF,
    TAG_FOOD_FIRST = 320
};
enum { PANEL_NONE, PANEL_STATUS, PANEL_ORDER, PANEL_SHOP, PANEL_CURFEW, PANEL_ALBUM, PANEL_ACTIVITY };
static struct
{
    pet_ui_action_fn action;
    pet_ui_point_fn point;
    lv_obj_t *campus, *gates, *door, *location, *location_text, *goal;
    pet_world_t world;
    int scene;
    unsigned panel;
    lv_obj_t *background, *shadow, *sprite, *cat_hit, *foreground, *marks, *hotspot[2];
    lv_obj_t *tone, *lamplight, *clock_text, *clock_date;
    int light_scene, light_band;
    lv_obj_t *layer, *food, *mood, *food_fill, *mood_fill, *state, *save_note;
    lv_obj_t *status_button, *bubble_text, *decor, *phone, *bag;
    lv_obj_t *audio_label, *audio_note, *volume_text, *link_detail, *link_name;
    lv_obj_t *status_dialog, *order_dialog, *order_note, *order_save, *order_button;
    lv_obj_t *shop_dialog, *shop_balance, *shop_hint, *checkin, *checkin_label, *shop_tab[2];
    lv_obj_t *shop_name[3], *shop_price[3], *shop_buy[3], *shop_action[3], *furniture[3];
    unsigned shop_page;
    bool delivery;
    unsigned selected;
    unsigned menu_window;
    lv_obj_t *wallet_text, *status_coins, *decor_entry, *curfew_dialog;
    lv_obj_t *menu_title, *menu_list, *menu_confirm_text, *menu_tracking, *menu_tab[4];
    lv_obj_t *menu_row[PE_MENU_COUNT], *menu_price[PE_MENU_COUNT];
    lv_obj_t *menu_name[PE_MENU_COUNT], *menu_image[PE_MENU_COUNT];
    lv_obj_t *blackboard, *memory_dialog, *memory_note, *memory_round, *memory_tile[4], *memory_footer;
    lv_obj_t *memory_title, *memory_symbol[4], *game_entry_label, *life_hit;
    lv_obj_t *challenge_track, *challenge_cursor;
    lv_obj_t *album_button, *album_dialog, *album_note, *album_footer, *album_list;
    lv_obj_t *album_row[12], *album_name[12], *album_status[12], *album_image[12];
    lv_obj_t *activity_dialog, *work_button, *canteen_order, *context_button, *context_label;
    unsigned building_kind;
    const char *room_override;
    unsigned pending_window;
} ui;
static bool audio_available = true;
static pet_link_t link_status = {PET_LINK_STARTING, "ShaheCat", 0};
static unsigned save_state = PET_SAVE_PENDING;
static void set_label(lv_obj_t *label, const char *value)
{
    if (strcmp(lv_label_get_text(label), value)) lv_label_set_text(label, value);
}
static void set_label_fmt(lv_obj_t *label, const char *format, ...)
{
    char value[192];
    va_list args;
    va_start(args, format);
    vsnprintf(value, sizeof(value), format, args);
    va_end(args);
    set_label(label, value);
}
static void set_image(lv_obj_t *image, const void *source)
{
    if (lv_image_get_src(image) != source) lv_image_set_src(image, source);
}

static const char *save_notes[] = {"等待自动保存", "已保存，重启可恢复", "存档暂不可用", "保存失败，稍后重试"};
void pet_ui_set_save_status(unsigned status)
{
    save_state = status <= PET_SAVE_FAILED ? status : PET_SAVE_UNAVAILABLE;
    if (ui.save_note) set_label(ui.save_note, save_notes[save_state]);
}
static void paint(void);
void pet_ui_set_audio_available(bool available)
{
    audio_available = available;
}
void pet_ui_set_link(const pet_link_t *link)
{
    if (!link) return;
    link_status = *link;
    link_status.name[sizeof(link_status.name)-1] = 0;
    if ((unsigned)link_status.state > PET_LINK_ERROR) link_status.state = PET_LINK_ERROR;
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
    set_label(o, s);
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
    if (cmd == TAG_SHOP)
    {
        if (ui.world.scene == PW_DORM && (ui.world.phase == PW_IDLE || ui.world.phase == PW_SLEEP)) ui.panel = PANEL_SHOP;
        paint(); return;
    }
    if (cmd >= TAG_FOOD_FIRST && cmd < TAG_FOOD_FIRST + PE_MENU_COUNT)
    {
        unsigned row = (unsigned)(cmd - TAG_FOOD_FIRST);
        if (!ui.delivery && ui.menu_window > 0)
        {
            if (row >= 2) return;
            ui.selected = (ui.menu_window - 1) * 2 + row;
        }
        else ui.selected = row;
        paint();
        return;
    }
    if (cmd >= TAG_WINDOW_FIRST && cmd < TAG_WINDOW_FIRST + 4)
    {
        ui.menu_window = (unsigned)(cmd - TAG_WINDOW_FIRST);
        ui.selected = ui.menu_window ? (ui.menu_window - 1) * 2 : 0;
        lv_obj_scroll_to_y(ui.menu_list, 0, LV_ANIM_OFF);
        paint();
        return;
    }
    if (cmd == TAG_CONFIRM && ui.panel == PANEL_ORDER)
    {
        pw_command_t choice = (pw_command_t)((ui.delivery ? PW_DELIVERY_FIRST : PW_MEAL_FIRST) + ui.selected);
        if (ui.action && ui.action(choice) == PW_OK) ui.panel = PANEL_NONE;
        paint(); return;
    }
    if (cmd == PW_DAILY_CHECKIN && ui.panel == PANEL_STATUS)
    { if (ui.action) ui.action(PW_DAILY_CHECKIN); paint(); return; }
    if (ui.panel == PANEL_SHOP && cmd >= PW_BUY_PLANT && cmd <= PW_BUY_SHELF)
    { if (ui.action) ui.action((pw_command_t)cmd); paint(); return; }
    if ((cmd == PW_SOUND_NEXT || cmd == PW_MUSIC_TOGGLE ||
         cmd == PW_VOLUME_DOWN || cmd == PW_VOLUME_UP) && ui.panel == PANEL_STATUS)
    {
        if (ui.action) ui.action((pw_command_t)cmd);
        return;
    }
    if (cmd == TAG_STATUS)
    {
        ui.panel = ui.panel == PANEL_STATUS ? PANEL_NONE : PANEL_STATUS;
        paint();
        return;
    }
    if (cmd == TAG_ALBUM)
    {
        if (ui.world.phase != PW_MINIGAME)
            ui.panel = ui.panel == PANEL_ALBUM ? PANEL_NONE : PANEL_ALBUM;
        paint();
        return;
    }
    if (cmd == TAG_ACTIVITY)
    {
        ui.room_override = NULL;
        if (ui.world.scene == PW_CAMPUS && ui.world.phase != PW_MINIGAME)
            ui.panel = ui.panel == PANEL_ACTIVITY ? PANEL_NONE : PANEL_ACTIVITY;
        paint();
        return;
    }
    if (ui.panel == PANEL_ACTIVITY &&
        (cmd == PW_ENTER_GYM || cmd == PW_DANCE || cmd == PW_BADMINTON))
    {
        if (cmd == PW_ENTER_GYM) { ui.room_override = NULL; ui.building_kind = 9; }
        if (ui.action && ui.action((pw_command_t)cmd) == PW_OK) ui.panel = PANEL_NONE;
        paint();
        return;
    }
    if (cmd == TAG_LIBRARY || cmd == TAG_PUBLIC || cmd == TAG_LAB || cmd == TAG_WISDOM)
    {
        static const char *names[] = {"图书馆 · 自习", "公共教学楼 · 教室",
                                      "教学实验综合楼 · 实验室", "智慧教学楼 · 自习"};
        ui.room_override = names[cmd - TAG_LIBRARY];
        ui.building_kind = (unsigned)(cmd - TAG_LIBRARY) == 0 ? 2 : (unsigned)(cmd - TAG_LIBRARY) + 2;
        if (ui.action) ui.action(PW_ENTER_CLASSROOM);
        paint(); return;
    }
    if (cmd == TAG_SOUTH)
    {
        ui.room_override = "雁南园 · 宿舍";
        ui.building_kind = 8;
        if (ui.action) ui.action(PW_ENTER_DORM);
        paint(); return;
    }
    if (cmd == TAG_FLAVOR || cmd == TAG_STAFF)
    {
        ui.room_override = cmd == TAG_FLAVOR ? "风味餐厅 · 窗口" : "教工餐厅 · 窗口";
        ui.building_kind = cmd == TAG_FLAVOR ? 6 : 7;
        ui.pending_window = cmd == TAG_FLAVOR ? 1 : 2;
        if (ui.action) ui.action(PW_ENTER_CANTEEN);
        paint(); return;
    }
    if (cmd == TAG_CLOSE)
    {
        if (ui.panel == PANEL_ORDER && !ui.delivery && ui.world.phase == PW_SEATED && ui.action)
            ui.action(PW_MEAL_LEAVE);
        ui.panel = PANEL_NONE;
        paint();
        return;
    }
    if (cmd == TAG_PHONE)
    {
        ui.delivery = true; ui.selected = 0; ui.menu_window = 0;
        ui.panel = PANEL_ORDER;
        lv_obj_scroll_to_y(ui.menu_list, 0, LV_ANIM_OFF);
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
    {
        if (cmd == PW_ENTER_DORM || cmd == PW_ENTER_CLASSROOM || cmd == PW_ENTER_CANTEEN)
            ui.room_override = NULL;
        if (cmd == PW_ENTER_DORM) ui.building_kind = 0;
        if (cmd == PW_ENTER_CLASSROOM) ui.building_kind = 1;
        if (cmd == PW_ENTER_CANTEEN) ui.building_kind = 8;
        if (cmd == PW_ENTER_CANTEEN) ui.pending_window = 0;
        if ((cmd == PW_SIT_MEAL || cmd == PW_EAT) && !ui.pending_window)
        {
            ui.menu_window = 0;
            ui.selected = 0;
        }
        ui.action(cmd == PW_EAT ? PW_SIT_MEAL : (pw_command_t)cmd);
    }
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
    set_image(o, src);
    lv_obj_set_pos(o, x, y);
    lv_obj_remove_flag(o, LV_OBJ_FLAG_CLICKABLE);
}
static const lv_image_dsc_t *const campus_building[] = {
    &shahe_building_0, &shahe_building_1, &shahe_building_2};
static const char *const campus_names[] = {"雁北园", "教综楼", "学生餐厅", "学生活动中心"};
static const pw_scene_t campus_rooms[] = {PW_DORM, PW_CLASSROOM, PW_CANTEEN, PW_GYM};

static void schematic_building(lv_obj_t *parent, int x, int y, int w, int h, uint32_t color, const char *name)
{
    lv_obj_t *b = box(parent, x, y, w, h, color, 9);
    lv_obj_set_style_bg_opa(b, LV_OPA_80, 0);
    lv_obj_set_style_border_width(b, 2, 0);
    lv_obj_set_style_border_color(b, lv_color_hex(0x8C7355), 0);
    lv_obj_t *l = text(b, name, 3, h / 2 - 10, &pet_font_16, HUD_INK);
    lv_obj_set_width(l, w - 6);
    lv_obj_set_style_text_align(l, LV_TEXT_ALIGN_CENTER, 0);
}

static void campus_label(lv_obj_t *parent, int x, int y, int width, const char *name)
{
    lv_obj_t *label = text(parent, name, x, y, &pet_font_16, HUD_INK);
    lv_obj_set_width(label, width);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_bg_color(label, lv_color_hex(HUD_PAPER), 0);
    lv_obj_set_style_bg_opa(label, LV_OPA_70, 0);
    lv_obj_set_style_radius(label, 5, 0);
    lv_obj_set_style_pad_ver(label, 2, 0);
    lv_obj_set_style_border_width(label, 1, 0);
    lv_obj_set_style_border_color(label, lv_color_hex(HUD_EDGE), 0);
    lv_obj_set_style_border_opa(label, LV_OPA_50, 0);
}

/* Yellow construction overlay: labels are visible, the area is not a walkable door. */
static void construction(lv_obj_t *parent, int x, int y, int w, int h, const char *name)
{
    lv_obj_t *area = box(parent, x, y, w, h, 0xF2C94C, 8);
    lv_obj_set_style_bg_opa(area, LV_OPA_70, 0);
    lv_obj_set_style_border_width(area, 2, 0);
    lv_obj_set_style_border_color(area, lv_color_hex(0xD9A441), 0);
    lv_obj_t *title = text(area, name, 4, 4, &pet_font_16, HUD_INK);
    lv_obj_set_width(title, w - 8);
    lv_obj_set_style_text_align(title, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_t *warn = text(area, "施工重地\n禁止进入", 4, h / 2 - 20, &pet_font_16, 0x6B4A12);
    lv_obj_set_width(warn, w - 8);
    lv_obj_set_style_text_align(warn, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_bg_color(warn, lv_color_hex(0xFFE08A), 0);
    lv_obj_set_style_bg_opa(warn, LV_OPA_70, 0);
    lv_obj_set_style_radius(warn, 4, 0);
}

/* The label sizes itself to its text. The parent remains the touch target. */
static lv_obj_t *scene_tag(lv_obj_t *parent, const char *name, lv_align_t align, int x, int y)
{
    lv_obj_t *label = text(parent, name, 0, 0, &pet_font_16, HUD_INK);
    lv_obj_set_style_bg_color(label, lv_color_hex(HUD_PAPER), 0);
    lv_obj_set_style_bg_opa(label, LV_OPA_80, 0);
    lv_obj_set_style_radius(label, 4, 0);
    lv_obj_set_style_border_width(label, 1, 0);
    lv_obj_set_style_border_color(label, lv_color_hex(HUD_EDGE), 0);
    lv_obj_set_style_border_opa(label, LV_OPA_60, 0);
    lv_obj_set_style_pad_left(label, 5, 0);
    lv_obj_set_style_pad_right(label, 5, 0);
    lv_obj_align(label, align, x, y);
    return label;
}

static void build_campus(lv_obj_t *screen)
{
    ui.campus = box(screen, 0, 0, PW_CAMPUS_WIDTH, PW_CAMPUS_HEIGHT, 0xB9CEA2, 0);
    lv_obj_add_flag(ui.campus, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(ui.campus, ground_click, LV_EVENT_CLICKED, NULL);
    for (unsigned y = 0; y < PW_CAMPUS_HEIGHT / 260; y++)
        for (unsigned x = 0; x < PW_CAMPUS_WIDTH / 280; x++)
            image(ui.campus, shahe_ground[y * (PW_CAMPUS_WIDTH / 280) + x], x * 280, y * 260);
    /* Schematic distribution from the BUPT Shahe map; yellow areas are off-limits. */
    construction(ui.campus, 395, 95, 150, 180, "学生宿舍IV区");
    construction(ui.campus, 560, 95, 640, 180, "科研楼");
    construction(ui.campus, 1210, 95, 125, 180, "智能工程学院");
    construction(ui.campus, 1210, 290, 125, 130, "计算机学院");
    construction(ui.campus, 55, 285, 140, 290, "体育馆");
    construction(ui.campus, 55, 1070, 245, 150, "研究生宿舍");
    construction(ui.campus, 395, 285, 150, 95, "北区食堂");
    schematic_building(ui.campus, 560, 440, 110, 60, 0xD5B7A0, "风味餐厅");
    schematic_building(ui.campus, 560, 560, 110, 75, 0xCBB79C, "教工餐厅");
    schematic_building(ui.campus, 600, 860, 100, 70, 0xC9B8A3, "雁南园");
    schematic_building(ui.campus, 1210, 700, 120, 80, 0xB8CDD0, "网络安全学院");
    schematic_building(ui.campus, 1210, 820, 120, 80, 0xC9C0D7, "数字媒体学院");
    schematic_building(ui.campus, 1210, 970, 120, 60, 0xD7C9B4, "人文学院");
    schematic_building(ui.campus, 1210, 1140, 120, 60, 0xC7D3C1, "工程实验楼");
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
    campus_label(ui.campus, 188, 600, 96, "运动场");
}

static void build_gates(lv_obj_t *screen)
{
    ui.gates = box(screen, 0, 0, PW_CAMPUS_WIDTH, PW_CAMPUS_HEIGHT, PAPER, 0);
    lv_obj_set_style_bg_opa(ui.gates, LV_OPA_TRANSP, 0);
    for (unsigned room = 0; room < sizeof(campus_rooms) / sizeof(campus_rooms[0]); room++)
    {
        pw_point_t door = pw_node(PW_CAMPUS, pw_campus_door(campus_rooms[room]));
        int y = (int)door.y - (room == PW_DORM ? 66 : 44);
        int tag = campus_rooms[room] == PW_GYM ? TAG_ACTIVITY : PW_ENTER_DORM + room;
        int w = campus_rooms[room] == PW_GYM ? 124 : 96;
        lv_obj_t *gate = button(ui.gates, (int)door.x - w / 2, y, w, 70, tag, PAPER, 5);
        lv_obj_set_style_bg_opa(gate, LV_OPA_TRANSP, 0);
        lv_obj_set_style_bg_opa(gate, LV_OPA_20, LV_STATE_PRESSED);
        scene_tag(gate, campus_names[room], LV_ALIGN_TOP_MID, 0, 3);
    }
}
static void build_buildings(lv_obj_t *screen)
{
    static const struct { int x, y, w, tag, label; const char *name; } entries[] = {
        {842, 520, 100, TAG_LIBRARY, 1, "图书馆"},
        {570, 780, 120, TAG_PUBLIC, 1, "公共教学楼"},
        {1150, 780, 150, TAG_LAB, 1, "教学实验综合楼"},
        {1150, 1110, 130, TAG_WISDOM, 1, "智慧教学楼"},
        {600, 860, 100, TAG_SOUTH, 0, ""},
        {560, 440, 110, TAG_FLAVOR, 0, ""},
        {560, 560, 110, TAG_STAFF, 0, ""}};
    for (unsigned i = 0; i < sizeof(entries) / sizeof(entries[0]); i++)
    {
        lv_obj_t *b = button(ui.gates, entries[i].x, entries[i].y, entries[i].w, 64,
                             entries[i].tag, PAPER, 6);
        lv_obj_set_style_bg_opa(b, LV_OPA_TRANSP, 0);
        lv_obj_set_style_bg_opa(b, LV_OPA_20, LV_STATE_PRESSED);
        if (entries[i].label) scene_tag(b, entries[i].name, LV_ALIGN_TOP_MID, 0, 3);
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
    {{36, 231, 105, 83, 117, 243, PW_SIT_MEAL, 5}},
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
    lv_obj_t *dialog = box(ui.layer, 43, 15, 304, 420, 0xF9FCFD, 18);
    ui.status_dialog = dialog;
    text(dialog, "小猫状态", 18, 13, &pet_font_20, INK);
    text(dialog, "v" PET_FIRMWARE_VERSION, 160, 18, &pet_font_16, BLUE);
    lv_obj_t *close = button(dialog, 254, 2, 44, 44, TAG_CLOSE, 0xF9FCFD, 12);
    lv_obj_t *x = text(close, "×", 0, 0, &pet_font_20, BLUE);
    lv_obj_center(x);
    box(dialog, 18, 48, 240, 1, 0xD4E0E5, 0);
    lv_obj_t *status = box(dialog, 32, 52, 240, 150, 0xF9FCFD, 0);
    text(status, "饱食", 0, 0, &pet_font_16, INK);
    ui.food = text(status, "70", 194, 0, &pet_font_20, INK);
    lv_obj_set_width(ui.food, 46);
    lv_obj_set_style_text_align(ui.food, LV_TEXT_ALIGN_RIGHT, 0);
    box(status, 0, 28, 240, 7, 0xE1E9ED, 3);
    ui.food_fill = box(status, 0, 28, 168, 7, 0xBF9769, 3);
    text(status, "心情", 0, 40, &pet_font_16, INK);
    ui.mood = text(status, "60", 194, 40, &pet_font_20, INK);
    lv_obj_set_width(ui.mood, 46);
    lv_obj_set_style_text_align(ui.mood, LV_TEXT_ALIGN_RIGHT, 0);
    box(status, 0, 68, 240, 7, 0xE1E9ED, 3);
    ui.mood_fill = box(status, 0, 68, 144, 7, 0x88AA92, 3);
    ui.state = text(status, "闲逛中", 0, 80, &pet_font_16, BLUE);
    ui.clock_date = text(status, "时间待校准", 0, 104, &pet_font_16, INK);
    ui.save_note = text(status, save_notes[save_state], 0, 128, &pet_font_16, 0x62736B);
    ui.link_detail = text(dialog, "", 18, 211, &pet_font_16, BLUE);
    ui.link_name = text(dialog, "", 18, 233, &pet_font_16, 0x62736B);
    lv_obj_t *less = button(dialog, 18, 299, 48, 44, PW_VOLUME_DOWN, 0xDCEAF3, 10);
    lv_obj_t *less_text = text(less, "−", 0, 0, &pet_font_20, INK);
    lv_obj_center(less_text);
    ui.volume_text = text(dialog, "", 78, 311, &pet_font_16, INK);
    lv_obj_t *more = button(dialog, 238, 299, 48, 44, PW_VOLUME_UP, 0xDCEAF3, 10);
    lv_obj_t *more_text = text(more, "+", 0, 0, &pet_font_20, INK);
    lv_obj_center(more_text);
    lv_obj_t *sound = button(dialog, 18, 353, 268, 44, PW_MUSIC_TOGGLE, 0xDCE6CB, 10);
    ui.audio_label = text(sound, "", 0, 0, &pet_font_16, INK);
    lv_obj_center(ui.audio_label);
    ui.audio_note = text(dialog, "", 18, 397, &pet_font_16, BLUE);
    ui.status_coins = text(dialog, "", 18, 252, &pet_font_16, INK);
    ui.checkin = button(dialog, 176, 247, 110, 44, PW_DAILY_CHECKIN, 0xE9D8A8, 10);
    ui.checkin_label = text(ui.checkin, "", 0, 0, &pet_font_16, INK);
    /* One shared in-scene menu, with independently checked model commands. */
    ui.order_dialog = box(ui.layer, 22, 18, 346, 414, PAPER, 18);
    ui.menu_title = text(ui.order_dialog, "", 18, 12, &pet_font_20, INK);
    lv_obj_t *menu_close = button(ui.order_dialog, 294, 2, 44, 44, TAG_CLOSE, PAPER, 12);
    lv_obj_t *menu_x = text(menu_close, "×", 0, 0, &pet_font_20, BLUE); lv_obj_center(menu_x);
    ui.order_note = text(ui.order_dialog, "", 18, 48, &pet_font_16, BLUE);
    /* Canteen windows: 全部 keeps the original six-row menu, the three windows show two each. */
    static const char *window_names[] = {"全部", "风味", "教工", "学生"};
    for (unsigned i = 0; i < 4; i++)
    {
        ui.menu_tab[i] = button(ui.order_dialog, 14 + (int)i * 76, 70, 68, 44,
                                TAG_WINDOW_FIRST + (int)i, 0xE6ECDE, 8);
        lv_obj_t *label = text(ui.menu_tab[i], window_names[i], 0, 0, &pet_font_16, INK);
        lv_obj_center(label);
    }
    ui.menu_list = box(ui.order_dialog, 12, 118, 322, 214, PAPER, 0);
    lv_obj_add_flag(ui.menu_list, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scroll_dir(ui.menu_list, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(ui.menu_list, LV_SCROLLBAR_MODE_AUTO);
    for (unsigned i = 0; i < PE_MENU_COUNT; i++)
    {
        ui.menu_row[i] = button(ui.menu_list, 2, i * 86, 310, 80, TAG_FOOD_FIRST + i, 0xE6ECDE, 10);
        lv_obj_t *clip = box(ui.menu_row[i], 8, 8, 64, 64, PAPER, 7);
        ui.menu_image[i] = lv_image_create(clip);
        lv_image_set_src(ui.menu_image[i], &pet_food_atlas);
        lv_obj_remove_flag(ui.menu_image[i], LV_OBJ_FLAG_CLICKABLE);
        ui.menu_name[i] = text(ui.menu_row[i], "", 80, 10, &pet_font_16, INK);
        ui.menu_price[i] = text(ui.menu_row[i], "", 80, 36, &pet_font_16, BLUE);
    }
    ui.menu_tracking = text(ui.order_dialog, "", 22, 128, &pet_font_20, INK);
    lv_obj_set_width(ui.menu_tracking, 302);
    ui.order_save = text(ui.order_dialog, "", 18, 342, &pet_font_16, BLUE);
    ui.order_button = button(ui.order_dialog, 18, 362, 310, 44, TAG_CONFIRM, 0xBCD4BC, 10);
    ui.menu_confirm_text = text(ui.order_button, "", 0, 0, &pet_font_16, INK);
    ui.shop_dialog = box(ui.layer, 28, 40, 334, 374, PAPER, 18);
    text(ui.shop_dialog, "布置宿舍", 18, 10, &pet_font_20, INK);
    lv_obj_t *decor_close = button(ui.shop_dialog, 282, 2, 44, 44, TAG_CLOSE, PAPER, 12);
    lv_obj_t *decor_x = text(decor_close, "×", 0, 0, &pet_font_20, BLUE); lv_obj_center(decor_x);
    ui.shop_balance = text(ui.shop_dialog, "", 18, 60, &pet_font_20, 0x967027);
    for (unsigned i = 0; i < 3; i++)
    {
        lv_obj_t *row = box(ui.shop_dialog, 18, 112 + i * 72, 298, 68, 0xE6ECDE, 10);
        lv_obj_t *preview = box(row, 8, 7, 60, 54, PAPER, 8);
        image(preview, &pet_furniture_preview[i], 3, 4);
        ui.shop_name[i] = text(row, "", 78, 7, &pet_font_16, INK);
        ui.shop_price[i] = text(row, "", 78, 36, &pet_font_16, BLUE);
        ui.shop_buy[i] = button(row, 192, 12, 96, 44, PW_BUY_PLANT + i, 0xDCEAF3, 9);
        ui.shop_action[i] = text(ui.shop_buy[i], "", 0, 0, &pet_font_16, INK);
    }
    ui.shop_hint = text(ui.shop_dialog, "", 18, 342, &pet_font_16, BLUE);
    lv_obj_set_width(ui.shop_hint, 298); lv_label_set_long_mode(ui.shop_hint, LV_LABEL_LONG_CLIP);
    /* Read-only dish album; it never sends a model command. */
    ui.album_dialog = box(ui.layer, 22, 18, 346, 414, PAPER, 18);
    text(ui.album_dialog, "校园图鉴", 18, 12, &pet_font_20, INK);
    lv_obj_t *album_close = button(ui.album_dialog, 294, 2, 44, 44, TAG_CLOSE, PAPER, 12);
    lv_obj_t *album_x = text(album_close, "×", 0, 0, &pet_font_20, BLUE);
    lv_obj_center(album_x);
    ui.album_note = text(ui.album_dialog, "", 18, 48, &pet_font_16, BLUE);
    /* 3x4 grid: all twelve dishes fit without scrolling (Catbook-style). */
    ui.album_list = box(ui.album_dialog, 12, 76, 322, 296, PAPER, 0);
    for (unsigned i = 0; i < 12; i++)
    {
        ui.album_row[i] = box(ui.album_list, 2 + (int)(i % 3) * 106, (int)(i / 3) * 74,
                              104, 74, 0xE6ECDE, 9);
        lv_obj_t *clip = box(ui.album_row[i], 28, 1, 48, 48, PAPER, 6);
        ui.album_image[i] = lv_image_create(clip);
        lv_image_set_src(ui.album_image[i], &pet_food_atlas);
        lv_obj_set_pos(ui.album_image[i], -(int)(i % 3) * 64 - 8, -(int)(i / 3) * 64 - 8);
        lv_obj_remove_flag(ui.album_image[i], LV_OBJ_FLAG_CLICKABLE);
        ui.album_name[i] = text(ui.album_row[i], "", 2, 50, &pet_font_16, INK);
        lv_obj_set_width(ui.album_name[i], 100);
        lv_obj_set_style_text_align(ui.album_name[i], LV_TEXT_ALIGN_CENTER, 0);
    }
    ui.album_footer = text(ui.album_dialog, "", 18, 382, &pet_font_16, 0x62736B);
    /* Student activity center: the campus gate opens this hub, not the gym directly. */
    ui.activity_dialog = box(ui.layer, 40, 105, 310, 240, PAPER, 18);
    text(ui.activity_dialog, "学生活动中心", 22, 12, &pet_font_20, INK);
    lv_obj_t *activity_close = button(ui.activity_dialog, 254, 2, 44, 44, TAG_CLOSE, PAPER, 12);
    lv_obj_t *activity_x = text(activity_close, "×", 0, 0, &pet_font_20, BLUE);
    lv_obj_center(activity_x);
    static const struct { const char *name; int cmd; } activities[] = {
        {"去健身房", PW_ENTER_GYM}, {"跳舞", PW_DANCE}, {"打羽毛球", PW_BADMINTON}};
    for (unsigned i = 0; i < 3; i++)
    {
        lv_obj_t *b = button(ui.activity_dialog, 22, 66 + (int)i * 54, 266, 44,
                             activities[i].cmd, i == 0 ? 0xBCD4BC : 0xE6ECDE, 10);
        lv_obj_t *label = text(b, activities[i].name, 0, 0, &pet_font_16, INK);
        lv_obj_center(label);
    }
    ui.curfew_dialog = box(ui.layer, 38, 105, 314, 240, PAPER, 18);
    text(ui.curfew_dialog, "宿舍宵禁", 22, 20, &pet_font_20, INK);
    text(ui.curfew_dialog, "23:00至06:00，宿舍门已关闭。", 22, 65, &pet_font_16, BLUE);
    text(ui.curfew_dialog, "可以在屋里活动，\n早上6点再出门。", 22, 98, &pet_font_16, INK);
    lv_obj_t *stay = button(ui.curfew_dialog, 22, 167, 270, 48, TAG_CLOSE, 0xBCD4BC, 10);
    lv_obj_t *stay_text = text(stay, "留在宿舍", 0, 0, &pet_font_16, INK); lv_obj_center(stay_text);

    ui.memory_dialog = box(ui.layer, 29, 75, 332, 312, PAPER, 18);
    ui.memory_title = text(ui.memory_dialog, "课堂记忆题", 18, 12, &pet_font_20, INK);
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
        ui.memory_symbol[i] = text(ui.memory_tile[i], symbols[i], 0, 0, &pet_font_20, INK);
        lv_obj_center(ui.memory_symbol[i]);
    }
    ui.memory_footer = text(ui.memory_dialog, "", 20, 281, &pet_font_16, BLUE);
    ui.challenge_track = box(ui.memory_dialog, 20, 126, 292, 20, 0xDCEAF3, 10);
    box(ui.challenge_track, 107, 0, 78, 20, 0x88AA92, 10);
    ui.challenge_cursor = box(ui.challenge_track, 0, 0, 5, 20, INK, 2);
    visible(ui.challenge_track, false);
    visible(ui.layer, false);
}
static void build_dorm_items(lv_obj_t *screen)
{
    ui.decor_entry = button(screen, 203, 279, 70, 58, TAG_SHOP, PAPER, 8);
    lv_obj_set_style_bg_opa(ui.decor_entry, LV_OPA_TRANSP, 0);
    scene_tag(ui.decor_entry, "布置", LV_ALIGN_BOTTOM_MID, 0, -4);

    ui.phone = button(screen, 118, 191, 44, 61, TAG_PHONE, PAPER, 7);
    lv_obj_set_style_bg_opa(ui.phone, LV_OPA_TRANSP, 0);
    box(ui.phone, 13, 9, 17, 25, 0x34596A, 4);
    box(ui.phone, 15, 11, 13, 19, 0xA4C1C0, 3);
    box(ui.phone, 19, 31, 5, 1, PAPER, 1);
    scene_tag(ui.phone, "外卖", LV_ALIGN_BOTTOM_MID, 0, -2);

    ui.bag = button(screen, 278, 248, 52, 51, PW_TAKEOUT, PAPER, 7);
    lv_obj_set_style_bg_opa(ui.bag, LV_OPA_TRANSP, 0);
    box(ui.bag, 19, 1, 14, 13, 0xA77952, 5);
    box(ui.bag, 22, 4, 8, 8, PAPER, 3);
    box(ui.bag, 10, 9, 32, 25, 0xC58D5D, 4);
    box(ui.bag, 13, 10, 24, 20, 0xE7B980, 3);
    scene_tag(ui.bag, "取餐", LV_ALIGN_BOTTOM_MID, 0, -1);
}

static void build_furniture(lv_obj_t *screen)
{
    /* Keep these items off the walkable floor. */
    static const int anchor[3][2] = {{194, 174}, {89, 166}, {32, 194}};
    for (unsigned i = 0; i < 3; i++)
    {
        lv_obj_t *prop = lv_image_create(screen);
        set_image(prop, &pet_furniture_room[i]);
        lv_obj_set_pos(prop, anchor[i][0], anchor[i][1]);
        lv_obj_remove_flag(prop, LV_OBJ_FLAG_CLICKABLE);
        ui.furniture[i] = prop;
    }
}
void pet_ui_create(pet_ui_action_fn callback)
{
    /* No UI-owned model or animation timer: recreation cannot duplicate settlement. */
    lv_obj_t *screen = lv_screen_active();
    lv_obj_clean(screen);
    memset(&ui, 0, sizeof(ui));
    ui.action = callback;
    ui.scene = -1;
    ui.light_scene = ui.light_band = -1;
    lv_obj_remove_flag(screen, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(screen, lv_color_hex(PAPER), 0);
    ui.background = lv_image_create(screen);
    lv_obj_add_flag(ui.background, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(ui.background, ground_click, LV_EVENT_CLICKED, NULL);
    lv_obj_set_pos(ui.background, 0, 0);
    set_image(ui.background, room_art[0]);
    ui.decor = lv_image_create(screen);
    lv_obj_remove_flag(ui.decor, LV_OBJ_FLAG_CLICKABLE);
    build_campus(screen);
    build_furniture(screen);
    ui.tone = box(screen, 0, 0, 390, 450, 0x17344E, 0);
    ui.lamplight = box(screen, 45, 177, 60, 60, 0xFFE6AA, LV_RADIUS_CIRCLE);
    lv_obj_set_style_bg_opa(ui.lamplight, LV_OPA_TRANSP, 0);
    ui.shadow = box(screen, 0, 0, 32, 8, 0x60523D, LV_RADIUS_CIRCLE);
    lv_obj_set_style_bg_opa(ui.shadow, LV_OPA_20, 0);
    ui.sprite = lv_image_create(screen);
    lv_obj_remove_flag(ui.sprite, LV_OBJ_FLAG_CLICKABLE);
    set_image(ui.sprite, pw_frames[0]);
    lv_image_set_pivot(ui.sprite, 36, 64);
    ui.cat_hit = button(screen, 0, 0, 64, 68, PW_TOUCH, PAPER, 0);
    lv_obj_set_style_bg_opa(ui.cat_hit, LV_OPA_TRANSP, 0);
    lv_obj_set_style_bg_opa(ui.cat_hit, LV_OPA_TRANSP, LV_STATE_PRESSED);
    ui.foreground = lv_image_create(screen);
    lv_obj_remove_flag(ui.foreground, LV_OBJ_FLAG_CLICKABLE);
    set_image(ui.foreground, &shahe_front_0);
    build_dorm_items(screen);
    ui.door = button(screen, 310, 100, 69, 143, PW_EXIT, PAPER, 5);
    lv_obj_set_style_bg_opa(ui.door, LV_OPA_TRANSP, 0);
    lv_obj_set_style_bg_opa(ui.door, LV_OPA_TRANSP, LV_STATE_PRESSED);
    scene_tag(ui.door, "出门", LV_ALIGN_BOTTOM_MID, 0, -7);
    build_gates(screen);
    build_buildings(screen);
    ui.blackboard = button(screen, 147, 108, 145, 71, PW_MEMORY_START, PAPER, 7);
    lv_obj_set_style_bg_opa(ui.blackboard, LV_OPA_TRANSP, 0);
    ui.game_entry_label = scene_tag(ui.blackboard, "记忆题", LV_ALIGN_TOP_RIGHT, -3, 3);
    ui.life_hit = button(screen, 10, 350, 64, 60, PW_LIFE, PAPER, 9);
    lv_obj_set_style_bg_opa(ui.life_hit, LV_OPA_TRANSP, 0);
    scene_tag(ui.life_hit, "看看", LV_ALIGN_RIGHT_MID, -2, 0);
    ui.work_button = button(screen, 10, 120, 92, 44, PW_WORKSTUDY, 0xE9D8A8, 9);
    lv_obj_set_style_bg_opa(ui.work_button, LV_OPA_80, 0);
    lv_obj_t *work_label = text(ui.work_button, "勤工俭学", 0, 0, &pet_font_16, INK);
    lv_obj_center(work_label);
    ui.canteen_order = button(screen, 250, 300, 104, 44, PW_SIT_MEAL, 0xE9D8A8, 9);
    lv_obj_set_style_bg_opa(ui.canteen_order, LV_OPA_80, 0);
    lv_obj_t *order_label = text(ui.canteen_order, "点餐", 0, 0, &pet_font_16, INK);
    lv_obj_center(order_label);
    ui.context_button = button(screen, 246, 196, 118, 44, PW_STUDY, 0xE6ECDE, 9);
    lv_obj_set_style_bg_opa(ui.context_button, LV_OPA_80, 0);
    ui.context_label = text(ui.context_button, "看书", 0, 0, &pet_font_16, INK);
    lv_obj_center(ui.context_label);
    ui.location = box(screen, 24, 28, 334, 54, HUD_PAPER, 9);
    lv_obj_set_style_bg_opa(ui.location, LV_OPA_90, 0);
    lv_obj_set_style_border_width(ui.location, 1, 0);
    lv_obj_set_style_border_color(ui.location, lv_color_hex(HUD_EDGE), 0);
    lv_obj_set_style_border_opa(ui.location, LV_OPA_60, 0);
    box(ui.location, 8, 12, 2, 30, HUD_EDGE, 1);
    box(ui.location, 195, 10, 1, 34, HUD_EDGE, 0);
    box(ui.location, 267, 10, 1, 34, HUD_EDGE, 0);
    ui.clock_text = text(ui.location, "--:-- 待校时", 16, 2, &pet_font_20, HUD_INK);
    ui.location_text = text(ui.location, room_names[0], 16, 28, &pet_font_16, HUD_INK);
    lv_obj_t *coin = box(ui.location, 204, 17, 20, 20, HUD_BRASS, 10);
    lv_obj_t *coin_mark = text(coin, "币", 0, 0, &pet_font_16, HUD_INK);
    lv_obj_center(coin_mark);
    ui.wallet_text = text(ui.location, "", 225, 16, &pet_font_16, HUD_INK);
    lv_obj_set_width(ui.wallet_text, 40);
    lv_obj_set_style_text_align(ui.wallet_text, LV_TEXT_ALIGN_CENTER, 0);
    ui.marks = text(screen, "", 0, 0, &pet_font_16, BLUE);
    ui.bubble_text = scene_tag(screen, "", LV_ALIGN_BOTTOM_MID, 0, -18);
    lv_obj_set_style_pad_left(ui.bubble_text, 11, 0);
    lv_obj_set_style_pad_right(ui.bubble_text, 11, 0);
    lv_obj_set_style_pad_top(ui.bubble_text, 2, 0);
    lv_obj_set_style_pad_bottom(ui.bubble_text, 2, 0);
    lv_obj_set_style_max_width(ui.bubble_text, 328, 0);
    lv_obj_set_style_text_align(ui.bubble_text, LV_TEXT_ALIGN_CENTER, 0);
    build_panel(screen);
    ui.status_button = button(screen, 296, 28, 62, 62, TAG_STATUS, HUD_PAPER, 8);
    lv_obj_set_style_bg_opa(ui.status_button, LV_OPA_TRANSP, 0);
    lv_obj_set_style_bg_opa(ui.status_button, LV_OPA_TRANSP, LV_STATE_PRESSED);
    lv_obj_t *book = box(ui.status_button, 24, 8, 14, 16, HUD_EDGE, 2);
    box(book, 3, 4, 8, 1, HUD_PAPER, 0);
    box(book, 3, 8, 6, 1, HUD_PAPER, 0);
    text(ui.status_button, "状态", 15, 28, &pet_font_16, HUD_INK);
    ui.album_button = button(screen, 296, 94, 62, 44, TAG_ALBUM, HUD_PAPER, 8);
    lv_obj_set_style_bg_opa(ui.album_button, LV_OPA_80, 0);
    text(ui.album_button, "图鉴", 15, 11, &pet_font_16, HUD_INK);
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

static void paint_checkin(void)
{
    const pet_world_t *w = &ui.world;
    pw_checkin_t state = pw_checkin_status(w);
    const char *label = state == PW_CHECKIN_WAIT_TIME ? "先校时" : state == PW_CHECKIN_DONE ? "今日已领"
        : state == PW_CHECKIN_FUTURE ? "校准日期" : state == PW_CHECKIN_FULL ? "余额已满"
        : w->phase != PW_IDLE ? "稍后签到" : "签到 +6";
    set_label(ui.checkin_label, label); lv_obj_center(ui.checkin_label);
    bool enabled = state == PW_CHECKIN_READY && w->phase == PW_IDLE;
    if (enabled) lv_obj_remove_state(ui.checkin, LV_STATE_DISABLED); else lv_obj_add_state(ui.checkin, LV_STATE_DISABLED);
    set_label_fmt(ui.status_coins, "生活费 %u 金币", w->wallet.coins);
}
static void paint_order(void)
{
    const pet_world_t *w = &ui.world;
    bool active = ui.delivery && w->order != PW_ORDER_NONE;
    bool canteen = !ui.delivery;
    unsigned count = canteen && ui.menu_window ? 2 : PE_MENU_COUNT;
    const pe_food_t *dish = pe_menu(ui.delivery, ui.selected);
    set_label(ui.menu_title, ui.delivery ? "宿舍外卖" : "学生餐厅 · 选窗口");
    visible(ui.menu_list, !active); visible(ui.menu_tracking, active);
    for (unsigned i = 0; i < 4; i++) visible(ui.menu_tab[i], canteen && !active);
    lv_obj_set_pos(ui.menu_list, 12, canteen ? 118 : 78);
    lv_obj_set_height(ui.menu_list, canteen ? 214 : 250);
    if (active)
    {
        set_label(ui.order_note, "订单进行中 · 菜品列表已收起");
        const pe_food_t *order = pe_order(w->order_choice);
        if (w->order == PW_ORDER_DELIVERING)
            set_label_fmt(ui.menu_tracking, "%s\n配送中 · 约 %u 秒\n送到宿舍门旁", order->name, (unsigned)((w->order_due - w->now + 999) / 1000));
        else set_label_fmt(ui.menu_tracking, "%s\n已经送到\n点门旁纸袋取餐", order->name);
    }
    else if (canteen) set_label_fmt(ui.order_note, "生活费 %u 金币 · 选窗口或全部", w->wallet.coins);
    else set_label_fmt(ui.order_note, "生活费 %u 金币 · 向上滑动看更多", w->wallet.coins);
    for (unsigned i=0; i<PE_MENU_COUNT; i++)
    {
        bool show = i < count;
        visible(ui.menu_row[i], show);
        if (!show) continue;
        unsigned idx = (canteen && ui.menu_window) ? (ui.menu_window - 1) * 2 + i : i;
        const pe_food_t *food = pe_menu(ui.delivery, idx);
        /* Names are separate labels so accessibility and hit testing see them. */
        set_label(ui.menu_name[i], food->name);
        unsigned tile = (ui.delivery ? PE_MENU_COUNT : 0) + idx;
        lv_obj_set_pos(ui.menu_image[i], -(int)(tile % 3) * 64, -(int)(tile / 3) * 64);
        set_label_fmt(ui.menu_price[i], "%u币 · 饱食+%u 心情+%u", food->price, food->food, food->mood);
        lv_obj_set_style_bg_color(ui.menu_row[i], lv_color_hex(idx==ui.selected ? 0xBCD4BC : 0xE6ECDE), 0);
    }
    for (unsigned i = 0; i < 4; i++)
        lv_obj_set_style_bg_color(ui.menu_tab[i], lv_color_hex(i == ui.menu_window ? 0xBCD4BC : 0xE6ECDE), 0);
    bool enabled = !active && w->food < 100 && w->wallet.coins >= dish->price &&
        (ui.delivery ? w->scene == PW_DORM && w->phase == PW_IDLE && w->cooldown[PW_TAKEOUT] <= w->now
                     : w->scene == PW_CANTEEN && w->phase == PW_SEATED && w->cooldown[PW_EAT] <= w->now);
    const char *reason = active ? save_notes[save_state] : w->food == 100 ? "吃饱啦，过会儿再来"
        : w->wallet.coins < dish->price ? "金币不足，可以换份便宜的"
        : ui.delivery && pw_busy(w) ? "等当前动作结束再下单"
        : (ui.delivery ? w->cooldown[PW_TAKEOUT] : w->cooldown[PW_EAT]) > w->now ? "稍等一下再来"
        : ui.delivery ? "确认才扣费 · 约30秒送达" : "选好再付款，取消不花钱";
    if (active) set_label(ui.order_save, w->order == PW_ORDER_DELIVERING ? "配送中，可先在屋里活动" : "已送达，点门旁纸袋取餐");
    else set_label(ui.order_save, reason);
    if (enabled) lv_obj_remove_state(ui.order_button, LV_STATE_DISABLED); else lv_obj_add_state(ui.order_button, LV_STATE_DISABLED);
    set_label_fmt(ui.menu_confirm_text, "%s · %u金币", ui.delivery ? "确认下单" : "确认用餐", dish->price);
    lv_obj_center(ui.menu_confirm_text); visible(ui.order_button, !active);
}
static void paint_shop(void)
{
    const pet_world_t *w = &ui.world;
    set_label_fmt(ui.shop_balance, "生活费  %u 金币", w->wallet.coins);
    for (unsigned i=0; i<3; i++)
    {
        pe_item_t item = (pe_item_t)(PE_PLANT+i); unsigned bit=1U<<i;
        bool owned=(w->wallet.owned&bit)!=0, placed=(w->wallet.equipped&bit)!=0;
        set_label(ui.shop_name[i], pe_name(item));
        if (owned) set_label(ui.shop_price[i], placed ? "已摆放" : "已购买");
        else set_label_fmt(ui.shop_price[i], "%u 金币", pe_price(item));
        const char *label = owned ? placed ? "收起" : "摆上" : "购买";
        bool enabled=w->scene==PW_DORM && w->phase==PW_IDLE && (owned || w->wallet.coins>=pe_price(item));
        if (w->phase!=PW_IDLE) label=w->phase==PW_SLEEP ? "先叫醒" : "稍后再来";
        else if (!owned && w->wallet.coins<pe_price(item)) label="金币不足";
        if (enabled) lv_obj_remove_state(ui.shop_buy[i],LV_STATE_DISABLED); else lv_obj_add_state(ui.shop_buy[i],LV_STATE_DISABLED);
        set_label(ui.shop_action[i],label);lv_obj_center(ui.shop_action[i]);
    }
    set_label(ui.shop_hint, "买后永久拥有，收起再摆上免费");
}
static void paint_album(void)
{
    const pet_world_t *w = &ui.world;
    unsigned got = pw_dish_count(w);
    set_label_fmt(ui.album_note, "已尝 %u / 12 道 · 吃过才会点亮", got);
    for (unsigned i = 0; i < 12; i++)
    {
        bool seen = (w->dishes & (1U << i)) != 0;
        const pe_food_t *dish = pe_menu(i >= PE_MENU_COUNT, i % PE_MENU_COUNT);
        set_label(ui.album_name[i], seen ? dish->name : "未解锁");
        lv_obj_set_style_text_color(ui.album_name[i], lv_color_hex(seen ? INK : 0x8A8578), 0);
        lv_obj_set_style_opa(ui.album_image[i], seen ? LV_OPA_COVER : LV_OPA_20, 0);
        lv_obj_set_style_bg_color(ui.album_row[i], lv_color_hex(seen ? 0xE6ECDE : 0xEFEDE3), 0);
    }
    set_label(ui.album_footer, got >= 12 ? "校园美食家：12 道全部尝过" : "上两行食堂 · 下两行外卖");
}
static void paint_memory(void)
{
    set_label(ui.memory_title, "课堂记忆题");
    visible(ui.challenge_track, false);
    static const char *symbols[] = {"花", "叶", "月", "星"};
    for (unsigned i = 0; i < 4; i++)
    {
        visible(ui.memory_tile[i], true);
        lv_obj_set_pos(ui.memory_tile[i], 20 + (i % 2) * 152, 111 + (i / 2) * 83);
        lv_obj_set_size(ui.memory_tile[i], 140, 72);
        set_label(ui.memory_symbol[i], symbols[i]); lv_obj_center(ui.memory_symbol[i]);
    }
    const pm_game_t *game = &ui.world.memory;
    const char *note = game->phase == PM_SHOW ? "先记住亮起的顺序"
                       : game->phase == PM_INPUT ? "按刚才的顺序点"
                       : game->phase == PM_WON ? "都记住啦！" : "差一点，下次再试";
    set_label(ui.memory_note, note);
    set_label_fmt(ui.memory_round, "第 %u / 3 轮 · %u 个图案", game->round + 1, game->length);
    int highlight = pm_highlight(game);
    static const uint32_t colors[] = {0xE4B9A1, 0xBFCEA5, 0xB5CDD2, 0xE5D49D};
    for (unsigned i = 0; i < 4; i++)
    {
        lv_obj_set_style_bg_color(ui.memory_tile[i], lv_color_hex(highlight == (int)i ? colors[i] : 0xE8E3D2), 0);
        lv_obj_set_style_border_width(ui.memory_tile[i], highlight == (int)i ? 3 : 0, 0);
        lv_obj_set_style_border_color(ui.memory_tile[i], lv_color_hex(BLUE), 0);
    }
    if (game->phase == PM_INPUT)
        set_label_fmt(ui.memory_footer, "已经想起 %u / %u 个", game->input_at, game->length);
    else if (game->phase == PM_WON)
        set_label_fmt(ui.memory_footer, "金币 +%d · 心情 %+d", ui.world.coin_gain, ui.world.mood_gain);
    else if (game->phase == PM_LOST)
        set_label_fmt(ui.memory_footer, "参与奖励 +%d 金币", ui.world.coin_gain);
    else
        set_label(ui.memory_footer, game->phase == PM_SHOW ? "先记住顺序，再依次点击"
                                                                  : "随时可以重新挑战");
}
static void paint_challenge(void)
{
    const pg_game_t *g = &ui.world.challenge;
    bool rhythm = g->kind == PG_RHYTHM;
    bool won = g->phase == PG_WON, lost = g->phase == PG_LOST;
    static const char *food[] = {"米饭", "青菜", "鸡蛋", "热汤"};
    static const char *objects[] = {"水杯", "书本", "钥匙", "耳机"};
    static const char *slots[] = {"位置一", "位置二", "位置三", "位置四"};
    set_label(ui.memory_title, rhythm ? "健身节奏" : g->kind == PG_FIND ? "宿舍找物" : "食堂配餐");
    if (won || lost) set_label(ui.memory_note, won ? "答对啦，完成挑战" : "差一点，下次再试");
    else if (rhythm) set_label(ui.memory_note, pg_window(g) ? "绿色时间，点一下！" : "等指针走到绿色");
    else if (g->phase == PG_SHOW) set_label(ui.memory_note, "先记住物品的位置");
    else set_label_fmt(ui.memory_note, "%s：%s", g->kind == PG_FIND ? "找一找" : "订单", 
        g->kind == PG_FIND ? objects[g->targets[g->round]] : food[g->targets[g->round]]);
    if (rhythm) set_label_fmt(ui.memory_round, "第 %u / 6 拍 · 命中 %u", g->round < 6 ? g->round + 1 : 6, g->score);
    else if (won || lost) set_label_fmt(ui.memory_round, "完成 %u / 3", g->score);
    else set_label_fmt(ui.memory_round, "%u / 3 · 剩余 %u 秒", g->score, g->total < 30000 ? (30000 - g->total + 999) / 1000 : 0);
    visible(ui.challenge_track, rhythm);
    lv_obj_set_x(ui.challenge_cursor, (int)(g->elapsed % 3000 * 287U / 3000));
    for (unsigned i = 0; i < 4; i++)
    {
        visible(ui.memory_tile[i], !rhythm || i == 0);
        lv_obj_set_pos(ui.memory_tile[i], rhythm ? 20 : 20 + (i % 2) * 152, rhythm ? 165 : 111 + (i / 2) * 83);
        lv_obj_set_size(ui.memory_tile[i], rhythm ? 292 : 140, rhythm ? 88 : 72);
        set_label(ui.memory_symbol[i], rhythm ? g->pressed ? "这一拍已点" : "跟上节拍" : g->kind == PG_MEAL ? food[i]
            : g->phase == PG_SHOW ? objects[g->cards[i]] : slots[i]);
        lv_obj_center(ui.memory_symbol[i]);
        lv_obj_set_style_bg_color(ui.memory_tile[i], lv_color_hex(rhythm && pg_window(g) ? 0x88AA92 : 0xDCEAF3), 0);
        lv_obj_set_style_border_width(ui.memory_tile[i], 0, 0);
    }
    if (won) set_label_fmt(ui.memory_footer, "金币 +%d · 心情 %+d", ui.world.coin_gain, ui.world.mood_gain);
    else if (lost) set_label_fmt(ui.memory_footer, "参与奖励 +%d 金币", ui.world.coin_gain);
    else set_label(ui.memory_footer,
        rhythm ? "每拍点一次 · 命中四拍即成功" : g->phase == PG_SHOW ? "五秒后物品会盖住" : "点选答案 · 随时可以退出");
}

static void paint_clock(void)
{
    pt_date_t date;
    bool valid=ui.world.clock.valid && pt_decode(ui.world.clock.utc,&date);
    pt_band_t band=valid?pt_band(ui.world.clock.utc):PT_DAY;
    static const char *bands[]={"早晨","白天","傍晚","夜晚"};
    if(valid){
        set_label_fmt(ui.clock_text,"%02u%c%02u · %s",date.hour,date.second%2?' ':':',date.minute,bands[band]);
        set_label_fmt(ui.clock_date,"%04u-%02u-%02u · %s",date.year,date.month,date.day,
            ui.world.clock.state==PT_SYNCED?(ui.world.clock.automatic?"自动校时":"手机校时"):"设备时钟");
    }else{
        set_label(ui.clock_text,"--:-- 待校时");
        set_label(ui.clock_date,ui.world.clock.state==PT_ERROR?"校时失败，请用手机重试"
            : link_status.state==PET_LINK_CONNECTED && link_status.auto_time==2?"自动校时未完成，可重连"
            : link_status.state==PET_LINK_CONNECTED && link_status.auto_time?"等待手机自动校时":"连接手机后可校准时间");
    }
    if(ui.light_band!=(int)band || ui.light_scene!=ui.scene){
        bool outdoor=ui.scene==PW_CAMPUS;
        static const uint32_t colors[]={0xFFF1CC,0xFFFFFF,0xD99B65,0x17344E};
        static const lv_opa_t opacities[]={24,0,40,112};
        lv_obj_set_style_bg_color(ui.tone,lv_color_hex(colors[band]),0);
        lv_obj_set_style_bg_opa(ui.tone,band==PT_NIGHT && outdoor ? 138 : opacities[band],0);
        lv_obj_set_style_bg_opa(ui.lamplight,band==PT_NIGHT && ui.scene==PW_DORM ? 32 : 0,0);
        ui.light_band=band;ui.light_scene=ui.scene;
    }
}
static void paint(void)
{
    if (ui.scene != ui.world.scene)
    {
        ui.scene = ui.world.scene;
        if (ui.scene != PW_DORM && (ui.panel == PANEL_ORDER || ui.panel == PANEL_SHOP))
            ui.panel = PANEL_NONE;
        build_hotspots();
        if (ui.scene != PW_CAMPUS)
        {
            lv_obj_remove_event_cb(ui.blackboard, click);
            pw_command_t cmd = ui.scene == PW_DORM ? PW_FIND_GAME : ui.scene == PW_CANTEEN ? PW_MEAL_GAME
                : ui.scene == PW_GYM ? PW_RHYTHM_GAME : PW_MEMORY_START;
            lv_obj_add_event_cb(ui.blackboard, click, LV_EVENT_CLICKED, (void *)(uintptr_t)cmd);
            set_label(ui.game_entry_label, ui.scene == PW_DORM ? "找物" : ui.scene == PW_CANTEEN ? "配餐"
                : ui.scene == PW_GYM ? "节奏" : "记忆题");
            static const int life_pos[][2] = {{10, 346}, {303, 307}, {308, 296}, {0, 0}, {280, 290}};
            lv_obj_set_pos(ui.life_hit, life_pos[ui.scene][0], life_pos[ui.scene][1]);
            lv_obj_set_height(ui.life_hit, 44);
        }
        if (ui.scene != PW_CAMPUS)
        {
            set_image(ui.background, room_art[ui.scene]);
            const lv_image_dsc_t *art = room_decor[ui.scene];
            set_image(ui.decor, art);
            lv_obj_set_pos(ui.decor, decor_anchor[ui.scene][0], decor_anchor[ui.scene][1] - art->header.h);
        }
    }
    paint_clock();
    bool campus = ui.world.scene == PW_CAMPUS;
    visible(ui.campus, campus);
    visible(ui.background, !campus);
    visible(ui.decor, !campus);
    for (unsigned i = 0; i < 3; i++)
        visible(ui.furniture[i], ui.world.scene == PW_DORM && (ui.world.wallet.equipped & (1U << i)));
    visible(ui.phone, ui.world.scene == PW_DORM);
    visible(ui.decor_entry, ui.world.scene == PW_DORM);
    visible(ui.blackboard, !campus && (ui.building_kind == 1 || ui.building_kind == 3 || ui.building_kind == 5));
    visible(ui.life_hit, !campus);
    visible(ui.work_button, ui.world.scene == PW_CLASSROOM && ui.world.phase != PW_MINIGAME &&
                            (ui.building_kind == 1 || ui.building_kind == 4));
    visible(ui.context_button, ui.world.scene == PW_CLASSROOM && ui.world.phase != PW_MINIGAME &&
                               (ui.building_kind == 2 || ui.building_kind == 4));
    visible(ui.canteen_order, ui.world.scene == PW_CANTEEN && ui.world.phase != PW_MINIGAME);
    visible(ui.status_button, ui.world.phase != PW_MINIGAME);
    visible(ui.location, ui.world.phase != PW_MINIGAME);
    visible(ui.bag, ui.world.scene == PW_DORM && ui.world.order == PW_ORDER_READY &&
                    ui.world.action != PW_TAKEOUT);
    visible(ui.door, !campus);
    if (!campus)
        set_label(ui.location_text, ui.room_override ? ui.room_override : room_names[ui.scene]);
    set_label(ui.context_label, ui.building_kind == 4 ? "做实验" : "看书");
    float cx, cy;
    pw_camera(&ui.world, &cx, &cy);
    int left = (int)lroundf(cx), top = (int)lroundf(cy);
    lv_obj_set_pos(ui.campus, -left, -top);
    lv_obj_set_pos(ui.gates, -left, -top);
    visible(ui.gates, campus);
    if (campus)
        set_label(ui.location_text, campus_area(ui.world.x, ui.world.y));
    visible(ui.goal, campus && ui.world.phase == PW_WALK && ui.world.purpose == 3);
    lv_obj_set_pos(ui.goal, (int)ui.world.point_goal.x - 10, (int)ui.world.point_goal.y - 10);
    pw_visual_t v;
    pw_visual(&ui.world, &v);
    v.x -= left;
    v.y -= top;
    set_image(ui.sprite, pw_frames[v.frame]);
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
        set_image(ui.foreground, bed ? &shahe_front_0 : front[ui.scene]);
        lv_obj_set_pos(ui.foreground, xy[0], xy[1]);
    }
    visible(ui.foreground, show_front);
    set_label(ui.marks,
                      v.sleeping ? "z z"
                                 : (ui.world.phase == PW_ACTION && ui.world.action == PW_TOUCH ? "♥" : ""));
    lv_obj_set_pos(ui.marks, limit((int)v.x + 18, 0, 350), limit((int)v.y - 51, 0, 420));
    set_label_fmt(ui.food, "%u", ui.world.food);
    set_label_fmt(ui.mood, "%u", ui.world.mood);
    lv_obj_set_width(ui.food_fill, ui.world.food ? ui.world.food * 240 / 100 : 1);
    lv_obj_set_width(ui.mood_fill, ui.world.mood ? ui.world.mood * 240 / 100 : 1);
    visible(ui.food_fill, ui.world.food > 0);
    visible(ui.mood_fill, ui.world.mood > 0);
    set_label(ui.state, ui.world.phase == PW_IDLE ? pw_need_text(&ui.world) : pw_state_text(&ui.world));
    lv_obj_set_width(ui.state, 240);
    lv_label_set_long_mode(ui.state, LV_LABEL_LONG_CLIP);
    static const char *links[] = {"蓝牙启动中", "蓝牙等待连接", "蓝牙已连接", "蓝牙异常"};
    set_label(ui.link_detail, links[link_status.state]);
    set_label(ui.link_name, link_status.name);
    uint32_t link_color = link_status.state == PET_LINK_CONNECTED ? 0x537958
        : link_status.state == PET_LINK_ERROR ? 0xA84D43 : BLUE;
    lv_obj_set_style_text_color(ui.link_detail, lv_color_hex(link_color), 0);
    set_label_fmt(ui.volume_text, "音量 %u / 10", ui.world.volume);
    set_label(ui.audio_label, ui.world.sound_mode == 2 ? "背景音乐：开" : "背景音乐：关");
    lv_obj_center(ui.audio_label);
    set_label(ui.audio_note, audio_available ? "调至零档无声 · 自动记住" : "声音暂不可用，切换重试");
    bool memory = ui.world.phase == PW_MINIGAME;
    visible(ui.layer, memory || ui.panel != PANEL_NONE);
    if (memory || ui.panel != PANEL_NONE) lv_obj_move_foreground(ui.layer);
    visible(ui.status_dialog, !memory && ui.panel == PANEL_STATUS);
    visible(ui.order_dialog, !memory && ui.panel == PANEL_ORDER);
    visible(ui.shop_dialog, !memory && ui.panel == PANEL_SHOP);
    visible(ui.curfew_dialog, !memory && ui.panel == PANEL_CURFEW);
    visible(ui.album_dialog, !memory && ui.panel == PANEL_ALBUM);
    visible(ui.activity_dialog, !memory && ui.panel == PANEL_ACTIVITY);
    visible(ui.album_button, ui.world.phase != PW_MINIGAME);
    paint_checkin();
    set_label_fmt(ui.wallet_text, "%u", ui.world.wallet.coins);
    visible(ui.memory_dialog, memory);
    if (memory) { if (ui.world.challenge.kind == PG_NONE) paint_memory(); else paint_challenge(); }
    if (ui.panel == PANEL_SHOP) paint_shop();
    if (ui.panel == PANEL_ALBUM) paint_album();
    if (ui.panel == PANEL_ORDER)
        paint_order();
    const char *msg = pw_message_text(&ui.world);
    if (!*msg && ui.world.phase == PW_IDLE) msg = pw_need_text(&ui.world);
    if (ui.world.now < 2300 && !*msg)
        msg = "v" PET_FIRMWARE_VERSION " · 沙河校园";
    bool gains = ui.world.now < ui.world.gain_until;
    if (ui.world.now < ui.world.coin_until)
        set_label_fmt(ui.bubble_text, "生活费 +%d 金币", ui.world.coin_gain);
    else if (gains && ui.world.gain && ui.world.mood_gain)
        set_label_fmt(ui.bubble_text, "饱食 %+d · 心情 %+d", ui.world.gain, ui.world.mood_gain);
    else if (gains && ui.world.gain)
        set_label_fmt(ui.bubble_text, "饱食 %+d", ui.world.gain);
    else if (gains)
        set_label_fmt(ui.bubble_text, "心情 %+d", ui.world.mood_gain);
    else
        set_label(ui.bubble_text, msg);
    visible(ui.bubble_text, gains || *msg || ui.world.now < ui.world.coin_until);
}
void pet_ui_update(const pet_world_t *world)
{
    if (world->curfew_events != ui.world.curfew_events) ui.panel = PANEL_CURFEW;
    else if (world->phase == PW_MINIGAME && ui.panel == PANEL_ALBUM) ui.panel = PANEL_NONE;
    else if (world->phase == PW_MINIGAME && ui.panel == PANEL_ACTIVITY) ui.panel = PANEL_NONE;
    else if (world->scene != PW_CAMPUS && ui.panel == PANEL_ACTIVITY) ui.panel = PANEL_NONE;
    else if (world->phase == PW_SEATED && ui.panel == PANEL_NONE && world->now - world->action_start >= 500)
    {
        ui.panel = PANEL_ORDER; ui.delivery = false;
        ui.menu_window = ui.pending_window;
        ui.selected = ui.menu_window ? (ui.menu_window - 1) * 2 : 0;
        ui.pending_window = 0;
        lv_obj_scroll_to_y(ui.menu_list, 0, LV_ANIM_OFF);
    }
    ui.world = *world;
    paint();
}
