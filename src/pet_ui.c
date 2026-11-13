/* Room views and continuous campus camera. Model owns movement and settlement. */
#include "pet_ui.h"
#include "pet_voice.h"
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
    TAG_SPORTS,
    TAG_BADMINTON_HIT,
    TAG_BADMINTON_CLOSE,
    TAG_LIBRARY_SHELF = 360,
    TAG_BOOK_FIRST,
    TAG_VOICE = 370,
    TAG_VOICE_BACK,
    TAG_VOICE_ACTION,
    TAG_SPORTS_ACTION = 350,
    TAG_SPORTS_CLOSE,
    TAG_SPORTS_RIGHT,
    TAG_COLLEGE_FIRST = 340,
    TAG_FOOD_FIRST = 320
};
enum { PANEL_NONE, PANEL_STATUS, PANEL_ORDER, PANEL_SHOP, PANEL_CURFEW, PANEL_ALBUM, PANEL_ACTIVITY, PANEL_SPORTS, PANEL_BADMINTON, PANEL_SPORTS_GAME, PANEL_LIBRARY };
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
    lv_obj_t *voice_entry, *voice_page, *voice_status, *voice_detail;
    lv_obj_t *voice_mic, *voice_action, *voice_action_text, *voice_wave[7];
    lv_obj_t *voice_volume_panel, *voice_volume_text, *voice_volume_bar[PV_VOLUME_COUNT];
    uint64_t voice_volume_until;
    bool voice_open;
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
    lv_obj_t *memory_icon[4], *projection_icon[PG_PROJECTION_STEPS];
    lv_obj_t *find_icon[4], *meal_icon[4];
    lv_obj_t *challenge_track, *challenge_cursor, *rhythm_beats[6];
    lv_obj_t *teaching_controls, *projection_pattern, *puzzle_tile[PG_GRID_CELLS], *puzzle_icon[PG_GRID_CELLS];
    lv_obj_t *circuit_wire[PG_CIRCUIT_LINKS], *circuit_trace;
    lv_point_precise_t circuit_points[PG_CIRCUIT_LINKS][2];
    lv_obj_t *album_button, *album_dialog, *album_note, *album_footer, *album_list;
    lv_obj_t *album_row[12], *album_name[12], *album_status[12], *album_image[12];
    lv_obj_t *activity_dialog, *work_button, *canteen_order, *context_button, *context_label;
    lv_obj_t *sports_dialog, *sports_game_dialog, *sports_run_layer, *sports_pull_layer, *sports_sit_layer;
    lv_obj_t *sports_action, *sports_right, *sports_score_label, *sports_timer, *sports_prompt, *sports_marker;
    lv_obj_t *sports_cat[3];
    bool sports_active, sports_finished;
    unsigned sports_mode, sports_score, sports_target, sports_distance, sports_last_cycle;
    uint64_t sports_started, sports_until, sports_feedback;
    lv_obj_t *badminton_dialog, *badminton_score, *badminton_timer, *badminton_hit, *badminton_note;
    lv_obj_t *badminton_zone, *badminton_shuttle, *badminton_action, *badminton_racket, *badminton_rival_racket;
    lv_obj_t *library_shelf, *library_panel, *library_book_name[5], *library_book_button[5];
    uint8_t library_borrowed;
    struct {
        uint64_t started, ends, early_until;
        unsigned phase, travel, player, rival;
        int impact_x, impact_y;
    } badminton;
    uint8_t building_id;
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

static void game_icon(lv_obj_t *icon, unsigned shape, uint32_t color)
{
    lv_obj_set_style_radius(icon, shape == 0 || shape == 3 ? LV_RADIUS_CIRCLE : 3, 0);
    lv_obj_set_style_transform_rotation(icon, shape == 2 ? 450 : 0, 0);
    lv_obj_set_style_bg_color(icon, lv_color_hex(color), 0);
    lv_obj_set_style_bg_opa(icon, shape == 3 ? LV_OPA_TRANSP : LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(icon, shape == 3 ? 3 : 0, 0);
    lv_obj_set_style_border_color(icon, lv_color_hex(color), 0);
}

static lv_obj_t *new_game_icon(lv_obj_t *parent, int size)
{
    lv_obj_t *icon = box(parent, 0, 0, size, size, INK, 3);
    lv_obj_set_style_transform_pivot_x(icon, size / 2, 0);
    lv_obj_set_style_transform_pivot_y(icon, size / 2, 0);
    lv_obj_center(icon);
    return icon;
}

static const uint32_t game_colors[] = {0xBC5149, 0x3D65A2, 0x287A6A, 0x956629};
static const unsigned challenge_dishes[] = {0, 3, 4, 5};

static lv_obj_t *find_object(lv_obj_t *parent, unsigned kind)
{
    lv_obj_t *icon = box(parent, 0, 0, 52, 52, PAPER, 0);
    lv_obj_set_style_bg_opa(icon, LV_OPA_TRANSP, 0);
    if (kind == 0)
    {
        box(icon, 12, 12, 24, 30, 0x397A93, 6);
        box(icon, 33, 16, 10, 15, 0x397A93, 4);
        box(icon, 35, 19, 5, 8, 0xDCEAF3, 2);
        box(icon, 16, 15, 16, 4, 0xA6D6DF, 2);
    }
    else if (kind == 1)
    {
        box(icon, 9, 11, 34, 31, 0xAD5C46, 4);
        box(icon, 14, 11, 12, 25, 0xF6E7C5, 2);
        box(icon, 27, 11, 12, 25, 0xFFF3D7, 2);
    }
    else if (kind == 2)
    {
        box(icon, 7, 12, 23, 23, 0xAA742A, LV_RADIUS_CIRCLE);
        box(icon, 12, 17, 13, 13, 0xDCEAF3, LV_RADIUS_CIRCLE);
        box(icon, 24, 21, 21, 7, 0xAA742A, 3);
        box(icon, 38, 27, 7, 10, 0xAA742A, 2);
    }
    else
    {
        box(icon, 9, 9, 34, 31, 0x39516D, 12);
        box(icon, 14, 15, 24, 28, 0xDCEAF3, 8);
        box(icon, 9, 24, 10, 17, 0x39516D, 4);
        box(icon, 33, 24, 10, 17, 0x39516D, 4);
    }
    return icon;
}
enum { BD_OFF, BD_SERVE, BD_INCOMING, BD_RETURN, BD_MISS, BD_DONE };

static bool badminton_can_hit(void)
{
    uint64_t elapsed = ui.world.now - ui.badminton.started;
    return ui.badminton.phase == BD_INCOMING && elapsed + 480 >= ui.badminton.travel &&
           elapsed < ui.badminton.travel && ui.world.now < ui.badminton.ends;
}

static void badminton_start(void)
{
    memset(&ui.badminton, 0, sizeof(ui.badminton));
    ui.badminton.phase = BD_SERVE;
    ui.badminton.started = ui.world.now;
    ui.badminton.ends = ui.world.now + 30000;
    ui.panel = PANEL_BADMINTON;
}

static void badminton_close(void)
{
    ui.badminton.phase = BD_OFF;
    if (ui.panel == PANEL_BADMINTON) ui.panel = PANEL_NONE;
    visible(ui.badminton_shuttle, false);
}

static void badminton_incoming(unsigned elapsed, int *x, int *y)
{
    unsigned t = elapsed < ui.badminton.travel ? elapsed * 100 / ui.badminton.travel : 100;
    *x = 303 - (int)(233 * t / 100);
    *y = 246 - (int)(120 * 4 * t * (100 - t) / 10000);
}

static void badminton_hit_event(void)
{
    if (ui.panel != PANEL_BADMINTON || ui.badminton.phase != BD_INCOMING) return;
    if (!badminton_can_hit())
    {
        ui.badminton.early_until = ui.world.now + 350;
        return;
    }
    badminton_incoming((unsigned)(ui.world.now - ui.badminton.started),
                       &ui.badminton.impact_x, &ui.badminton.impact_y);
    ui.badminton.player++;
    ui.badminton.phase = BD_RETURN;
    ui.badminton.started = ui.world.now;
}

static void badminton_tick(void)
{
    if (ui.panel != PANEL_BADMINTON || ui.badminton.phase == BD_OFF || ui.badminton.phase == BD_DONE) return;
    uint64_t now = ui.world.now, elapsed = now - ui.badminton.started;
    if (now >= ui.badminton.ends)
    {
        ui.badminton.phase = BD_DONE;
        return;
    }
    if (ui.badminton.phase == BD_SERVE && elapsed >= 550)
    {
        static const unsigned pace[] = {1650, 1430, 1570};
        ui.badminton.travel = pace[(ui.badminton.player + ui.badminton.rival) % 3];
        ui.badminton.phase = BD_INCOMING;
        ui.badminton.started = now;
    }
    else if (ui.badminton.phase == BD_INCOMING && elapsed >= ui.badminton.travel)
    {
        ui.badminton.rival++;
        ui.badminton.phase = BD_MISS;
        ui.badminton.started = now;
    }
    else if ((ui.badminton.phase == BD_RETURN || ui.badminton.phase == BD_MISS) && elapsed >= 650)
    {
        ui.badminton.phase = ui.badminton.player + ui.badminton.rival >= 10 ? BD_DONE : BD_SERVE;
        ui.badminton.started = now;
    }
}

static void paint_badminton(void)
{
    unsigned phase = ui.badminton.phase;
    uint64_t now = ui.world.now;
    unsigned elapsed = (unsigned)(now - ui.badminton.started);
    bool can_hit = badminton_can_hit();
    bool incoming = phase == BD_INCOMING;
    bool flying = incoming || phase == BD_RETURN;
    if (flying)
    {
        int x, y;
        if (incoming) badminton_incoming(elapsed, &x, &y);
        else
        {
            int t = elapsed < 650 ? (int)(elapsed * 100 / 650) : 100;
            x = (ui.badminton.impact_x * (100 - t) + 303 * t) / 100;
            y = (ui.badminton.impact_y * (100 - t) + 246 * t) / 100 - 95 * 4 * t * (100 - t) / 10000;
        }
        lv_obj_set_pos(ui.badminton_shuttle, x - 12, y - 8);
        lv_obj_set_style_transform_rotation(ui.badminton_shuttle, incoming ? -160 : 160, 0);
    }
    visible(ui.badminton_shuttle, flying);
    visible(ui.badminton_hit, phase != BD_DONE);
    lv_obj_set_style_bg_color(ui.badminton_hit, lv_color_hex(can_hit ? 0xEEC16C : 0x294E43), 0);
    lv_obj_set_style_text_color(ui.badminton_action, lv_color_hex(can_hit ? 0x23352E : 0xFFF1D5), 0);
    lv_obj_set_style_bg_opa(ui.badminton_zone, can_hit ? LV_OPA_40 : LV_OPA_10, 0);
    lv_obj_set_style_border_opa(ui.badminton_zone, can_hit ? LV_OPA_100 : LV_OPA_50, 0);
    lv_obj_set_style_transform_rotation(ui.badminton_racket, phase == BD_RETURN && elapsed < 220 ? -520 : -120, 0);
    lv_obj_set_style_transform_rotation(ui.badminton_rival_racket, phase == BD_SERVE ? 220 : 140, 0);
    set_label_fmt(ui.badminton_score, "你  %u  :  %u  对手", ui.badminton.player, ui.badminton.rival);
    unsigned remaining = now < ui.badminton.ends ? (unsigned)((ui.badminton.ends - now + 999) / 1000) : 0;
    set_label_fmt(ui.badminton_timer, "%u / 10 球   %u 秒", ui.badminton.player + ui.badminton.rival, remaining);
    lv_obj_align(ui.badminton_score, LV_ALIGN_TOP_MID, 0, 12);
    lv_obj_align(ui.badminton_timer, LV_ALIGN_TOP_MID, 0, 42);
    const char *note = "看球，等击球区亮起";
    if (phase == BD_DONE)
        note = ui.badminton.player > ui.badminton.rival ? "比赛结束 · 你赢了" :
               ui.badminton.player == ui.badminton.rival ? "比赛结束 · 平局" : "比赛结束 · 对手赢了";
    else if (phase == BD_RETURN) note = "打回去了！";
    else if (phase == BD_MISS) note = "漏球 · 对手得分";
    else if (can_hit) note = "现在挥拍！点亮区";
    else if (incoming && now < ui.badminton.early_until) note = "早了，再等一下";
    set_label(ui.badminton_note, note);
    lv_obj_align(ui.badminton_note, LV_ALIGN_TOP_MID, 0, 351);
}

static void sports_paint(void)
{
    unsigned mode = ui.sports_mode;
    unsigned elapsed = (unsigned)(ui.world.now - ui.sports_started);
    unsigned left = ui.world.now < ui.sports_until
        ? (unsigned)((ui.sports_until - ui.world.now + 999) / 1000) : 0;
    bool hit = ui.world.now < ui.sports_feedback;
    const uint32_t ready = 0xEEC16C, waiting = 0x527566;
    bool window = mode == 1 ? ui.sports_target == 0 :
        mode == 2 ? (elapsed % 1400 >= 450 && elapsed % 1400 < 950) :
        elapsed % 1600 >= 800 && elapsed % 1600 < 1400;
    const unsigned target = mode == 1 ? 100 : mode == 2 ? 10 : 12;
    const unsigned score = mode == 1 ? ui.sports_distance : ui.sports_score;
    const char *title = mode == 1 ? "跑步" : mode == 2 ? "引体向上" : "仰卧起坐";
    set_label_fmt(ui.sports_score_label, "%s  %u / %u", title, score, target);
    set_label_fmt(ui.sports_timer, "剩余 %u 秒", left);
    visible(ui.sports_action, ui.sports_active);
    visible(ui.sports_right, ui.sports_active && mode == 1);
    lv_obj_set_style_bg_color(ui.sports_action, lv_color_hex(window ? ready : waiting), 0);
    lv_obj_set_style_bg_color(ui.sports_right, lv_color_hex(window ? waiting : ready), 0);
    const char *hint = mode == 1 ? "左右依次点" :
        mode == 2 ? "指针到绿区时点小猫" : "小猫起身时点一下";
    set_label(ui.sports_prompt, ui.sports_finished ? (score == target ? "挑战完成" : "时间到") : hint);
    if (mode == 1)
    {
        set_image(ui.sports_cat[0], pw_frames[12 + (hit && ui.sports_target)]);
        lv_obj_set_pos(ui.sports_cat[0], 16 + (int)score * 282 / 100, 171 - (hit ? 5 : 0));
    }
    else if (mode == 2)
    {
        lv_obj_set_y(ui.sports_marker, 84 + (int)(elapsed % 1400) * 162 / 1400);
        set_image(ui.sports_cat[1], shahe_sports_poses[hit ? 1 : 0]);
        lv_obj_set_y(ui.sports_cat[1], hit ? 84 : 102);
    }
    else
    {
        set_image(ui.sports_cat[2], shahe_sports_poses[window || hit ? 3 : 2]);
        lv_obj_set_y(ui.sports_cat[2], 146);
    }
}

static void sports_start(int cmd)
{
    if (!ui.action || ui.action((pw_command_t)cmd) != PW_OK) return;
    ui.sports_active = true;
    ui.sports_finished = false;
    ui.panel = PANEL_SPORTS_GAME;
    ui.sports_mode = cmd == PW_RUN ? 1 : cmd == PW_PULLUP ? 2 : 3;
    ui.sports_score = ui.sports_target = ui.sports_distance = 0;
    ui.sports_last_cycle = UINT32_MAX;
    ui.sports_started = ui.world.now;
    ui.sports_until = ui.world.now + 20000;
    ui.sports_feedback = 0;
    visible(ui.sports_run_layer, ui.sports_mode == 1);
    visible(ui.sports_pull_layer, ui.sports_mode == 2);
    visible(ui.sports_sit_layer, ui.sports_mode == 3);
    lv_obj_set_pos(ui.sports_action, ui.sports_mode == 1 ? 45 : 126,
                   ui.sports_mode == 1 ? 382 : ui.sports_mode == 2 ? 148 : 202);
    lv_obj_set_size(ui.sports_action, ui.sports_mode == 1 ? 132 : 126,
                    ui.sports_mode == 1 ? 52 : ui.sports_mode == 2 ? 156 : 118);
    lv_obj_set_style_bg_opa(ui.sports_action, ui.sports_mode == 1 ? LV_OPA_COVER : LV_OPA_10, 0);
    lv_obj_set_style_border_width(ui.sports_action, ui.sports_mode == 1 ? 0 : 2, 0);
    lv_obj_set_style_border_color(ui.sports_action, lv_color_hex(0xEEC16C), 0);
    visible(lv_obj_get_child(ui.sports_action, 0), ui.sports_mode == 1);
    sports_paint();
}

static void sports_end(void)
{
    ui.sports_active = false;
    ui.sports_finished = true;
}

static void sports_tick(void)
{
    if (ui.panel != PANEL_SPORTS_GAME) return;
    if (ui.sports_active && ui.world.now >= ui.sports_until) sports_end();
    sports_paint();
}

static void sports_hit(bool right)
{
    if (!ui.sports_active || ui.panel != PANEL_SPORTS_GAME) return;
    if (ui.world.now >= ui.sports_until) { sports_end(); return; }
    unsigned elapsed = (unsigned)(ui.world.now - ui.sports_started);
    if (ui.sports_mode == 1)
    {
        if (right != ui.sports_target) return;
        ui.sports_target ^= 1;
        ui.sports_distance += 7;
        if (ui.sports_distance >= 100) { ui.sports_distance = 100; sports_end(); }
    }
    else
    {
        unsigned period = ui.sports_mode == 2 ? 1400 : 1600;
        unsigned phase = elapsed % period, cycle = elapsed / period;
        unsigned from = ui.sports_mode == 2 ? 450 : 800;
        unsigned to = ui.sports_mode == 2 ? 950 : 1400;
        if (right || cycle == ui.sports_last_cycle || phase < from || phase >= to) return;
        ui.sports_last_cycle = cycle;
        if (++ui.sports_score == (ui.sports_mode == 2 ? 10U : 12U)) sports_end();
    }
    ui.sports_feedback = ui.world.now + 200;
    sports_paint();
}

static void set_voice_page(bool open)
{
    ui.voice_open = open;
    ui.voice_volume_until = 0;
    pet_voice_page(open);
    paint();
}

static bool can_open_voice(void)
{
    return !ui.voice_open && ui.panel == PANEL_NONE && ui.world.phase != PW_MINIGAME;
}

void pet_ui_key2(pet_key2_event_t event)
{
    if (event == PET_KEY2_LONG && can_open_voice())
        set_voice_page(true);
    else if (event == PET_KEY2_SHORT && ui.voice_open)
    {
        pet_voice_volume_t setting = pet_voice_cycle_volume();
        unsigned volume = setting == PV_VOLUME_MUTE ? 0 : setting + 1;
        ui.voice_volume_until = ui.world.now + 2000;
        set_label_fmt(ui.voice_volume_text, "%u/3", volume);
        lv_obj_align(ui.voice_volume_text, LV_ALIGN_TOP_MID, 0, 144);
        for (unsigned i = 0; i < PV_VOLUME_COUNT; i++)
            lv_obj_set_style_bg_color(ui.voice_volume_bar[i],
                lv_color_hex(i < volume ? 0xEAB07B : 0x567487), 0);
        visible(ui.voice_volume_panel, true);
    }
}

static void click(lv_event_t *e)
{
    lv_obj_t *o = lv_event_get_target(e);
    if (lv_obj_has_state(o, LV_STATE_DISABLED))
        return;
    int cmd = (int)(uintptr_t)lv_event_get_user_data(e);
    if (cmd == TAG_VOICE || cmd == TAG_VOICE_BACK)
    {
        if (cmd == TAG_VOICE_BACK || can_open_voice()) set_voice_page(cmd == TAG_VOICE);
        return;
    }
    if (cmd == TAG_VOICE_ACTION)
    {
        pet_voice_state_t state = pet_voice_view().state;
        if (state == PV_PAIR) pet_voice_pair();
        else if (state == PV_WAITING || state == PV_PLAYING) pet_voice_cancel();
        else if (state == PV_ACTIVATE || state == PV_ERROR) pet_voice_retry();
        return;
    }
    if (cmd == TAG_MEMORY_CLOSE || (cmd == TAG_CLOSE && ui.world.phase == PW_MINIGAME))
    {
        if (ui.action) ui.action(PW_MEMORY_CANCEL);
        return;
    }
    if (ui.world.phase == PW_MINIGAME)
    {
        bool choice = cmd >= PW_MEMORY_0 && cmd <= PW_MEMORY_3;
        bool tile = cmd >= PW_GRID_FIRST && cmd <= PW_GRID_LAST;
        if ((choice || tile) && ui.action) ui.action((pw_command_t)cmd);
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
        if (ui.world.scene == PW_CAMPUS && ui.world.phase != PW_MINIGAME)
            ui.panel = ui.panel == PANEL_ACTIVITY ? PANEL_NONE : PANEL_ACTIVITY;
        paint();
        return;
    }
    if (ui.panel == PANEL_ACTIVITY &&
        (cmd == PW_ENTER_GYM || cmd == PW_DANCE || cmd == PW_BADMINTON))
    {
        if (cmd == PW_BADMINTON)
        {
            if (ui.action && ui.action(PW_BADMINTON) == PW_OK &&
                ui.world.scene == PW_CAMPUS && ui.panel == PANEL_ACTIVITY)
                badminton_start();
            paint(); return;
        }
        if (ui.action && ui.action((pw_command_t)cmd) == PW_OK) ui.panel = PANEL_NONE;
        paint();
        return;
    }
    if (cmd == TAG_LIBRARY || cmd == TAG_PUBLIC || cmd == TAG_LAB || cmd == TAG_WISDOM)
    {
        if (ui.action) ui.action((pw_command_t)(PW_ENTER_LIBRARY + (cmd - TAG_LIBRARY)));
        paint(); return;
    }
    if (cmd == TAG_SPORTS)
    {
        if (ui.world.scene == PW_CAMPUS && ui.world.phase != PW_MINIGAME)
            ui.panel = ui.panel == PANEL_SPORTS ? PANEL_NONE : PANEL_SPORTS;
        paint(); return;
    }
    if (ui.panel == PANEL_SPORTS &&
        (cmd == PW_RUN || cmd == PW_PULLUP || cmd == PW_SITUP))
    {
        sports_start(cmd);
        paint(); return;
    }
    if (cmd == TAG_SPORTS_ACTION || cmd == TAG_SPORTS_RIGHT)
    { sports_hit(cmd == TAG_SPORTS_RIGHT); paint(); return; }
    if (cmd == TAG_SPORTS_CLOSE)
    {
        ui.sports_active = false; ui.sports_finished = false;
        ui.panel = PANEL_NONE; paint(); return;
    }
    if (cmd == TAG_SOUTH)
    {
        if (ui.action) ui.action(PW_SOUTH_LOCK);
        paint(); return;
    }
    if (cmd == TAG_FLAVOR || cmd == TAG_STAFF)
    {
        if (ui.action) ui.action(cmd == TAG_FLAVOR ? PW_ENTER_FLAVOR : PW_ENTER_STAFF);
        paint(); return;
    }
    if (cmd == TAG_LIBRARY_SHELF)
    {
        if (pw_in_library(&ui.world))
            ui.panel = ui.panel == PANEL_LIBRARY ? PANEL_NONE : PANEL_LIBRARY;
        paint(); return;
    }
    if (cmd >= TAG_BOOK_FIRST && cmd < TAG_BOOK_FIRST + 5 && ui.panel == PANEL_LIBRARY)
    {
        unsigned i = (unsigned)(cmd - TAG_BOOK_FIRST);
        ui.library_borrowed ^= (uint8_t)(1U << i);
        paint(); return;
    }
    if (cmd == TAG_BADMINTON_HIT) { badminton_hit_event(); paint(); return; }
    if (cmd == TAG_BADMINTON_CLOSE)
    {
        badminton_close();
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
enum { CENTER_X = 665, CENTER_Y = 558, LIBRARY_X = 907, LIBRARY_Y = 477,
       WISDOM_X = 1044, WISDOM_Y = 1120 };

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
    construction(ui.campus, 55, 285, 140, 290, "体育馆");
    construction(ui.campus, 55, 1070, 245, 150, "研究生宿舍");
    construction(ui.campus, 395, 285, 150, 95, "北区食堂");
    construction(ui.campus, 560, 95, 640, 180, "科研楼");
    construction(ui.campus, 1210, 95, 125, 180, "智能工程学院");
    construction(ui.campus, 1210, 290, 125, 130, "计算机学院");
    static const int16_t trees[][2] = {
        {65,136}, {146,111}, {245,142}, {105,249}, {249,250},
        {590,121}, {691,117}, {875,112}, {996,123}, {1090,150},
        {590,321}, {688,338}, {885,318}, {988,327}, {1085,304},
        {116,443}, {93,548}, {279,388}, {282,621}, {450,373},
        {583,444}, {603,562}, {843,531}, {1102,555}, {1242,475},
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
    image(ui.campus, &shahe_library, LIBRARY_X, LIBRARY_Y);
    image(ui.campus, &shahe_center, CENTER_X, CENTER_Y);
    image(ui.campus, &shahe_lecture, 617, 751);
    image(ui.campus, &shahe_lecture, 1215, 750);
    image(ui.campus, &shahe_lecture, WISDOM_X, WISDOM_Y);
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
    /* Coordinates are on the building side of the road. Only each sign takes taps. */
    static const struct
    {
        int x, y, width, tag;
        const char *name;
    } signs[] = {
        {455, 555, 72, PW_ENTER_DORM, "雁北园"},
        {1020, 781, 72, PW_ENTER_CLASSROOM, "教综楼"},
        {625, 1135, 82, PW_ENTER_CANTEEN, "学生餐厅"},
        {715, 584, 74, TAG_ACTIVITY, "学生\n活动中心"},
        {950, 500, 72, TAG_LIBRARY, "图书馆"},
        {1103, 1118, 74, TAG_WISDOM, "智慧\n教学楼"},
        {673, 775, 74, TAG_PUBLIC, "公共\n教学楼"},
        {1270, 756, 82, TAG_LAB, "教学实验\n综合楼"},
        {188, 605, 72, TAG_SPORTS, "运动场"}
    };
    for (unsigned i = 0; i < sizeof(signs) / sizeof(signs[0]); i++)
    {
        int width = signs[i].width;
        lv_obj_t *sign = button(ui.gates, signs[i].x - width / 2, signs[i].y - 22,
                                width, 44, signs[i].tag, HUD_PAPER, 6);
        lv_obj_set_style_bg_opa(sign, LV_OPA_90, 0);
        lv_obj_set_style_border_width(sign, 1, 0);
        lv_obj_set_style_border_color(sign, lv_color_hex(HUD_EDGE), 0);
        lv_obj_t *label = text(sign, signs[i].name, 0, 0, &pet_font_16, HUD_INK);
        lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_center(label);
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
static const lv_image_dsc_t *const classroom_art[] = {
    [PW_PUBLIC_BUILDING] = &shahe_public_room,
    [PW_LAB_BUILDING] = &shahe_lab_room,
    [PW_SMART_BUILDING] = &shahe_smart_room};
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
    if (ui.world.scene == PW_CAMPUS || pw_in_library(&ui.world) ||
        (ui.world.scene == PW_CLASSROOM && ui.world.building_id == PW_LAB_BUILDING))
        return;
    static const furniture_hotspot_t public_desk = {13, 261, 224, 49, 55, 273, PW_STUDY, 4};
    unsigned count = ui.world.scene == PW_DORM ? 2 : 1;
    for (unsigned t = 0; t < count; t++)
    {
        const furniture_hotspot_t *f = &furniture[ui.world.scene][t];
        if (ui.world.scene == PW_CLASSROOM && ui.world.building_id == PW_PUBLIC_BUILDING)
            f = &public_desk;
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
static lv_obj_t *badminton_racket(lv_obj_t *parent, int x, int y, uint32_t color)
{
    lv_obj_t *racket = box(parent, x, y, 27, 64, 0, 0);
    lv_obj_set_style_bg_opa(racket, LV_OPA_TRANSP, 0);
    lv_obj_set_style_transform_pivot_x(racket, 13, 0);
    lv_obj_set_style_transform_pivot_y(racket, 53, 0);
    box(racket, 2, 0, 23, 32, color, 12);
    box(racket, 5, 3, 17, 26, 0xD6DCC5, 9);
    for (int i = 1; i <= 3; i++) box(racket, 7, 5 + i * 5, 13, 1, 0x9CA492, 0);
    box(racket, 12, 6, 2, 19, 0x9CA492, 0);
    box(racket, 11, 31, 5, 22, color, 2);
    box(racket, 9, 47, 9, 15, 0x5A3D2B, 4);
    return racket;
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
    ui.menu_list = box(ui.order_dialog, 12, 118, 322, 202, PAPER, 0);
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
    ui.order_save = text(ui.order_dialog, "", 18, 324, &pet_font_16, BLUE);
    ui.order_button = button(ui.order_dialog, 12, 346, 322, 60, TAG_CONFIRM, 0xBCD4BC, 10);
    ui.menu_confirm_text = text(ui.order_button, "", 0, 0, &pet_font_20, INK);
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
    ui.sports_dialog = box(ui.layer, 38, 100, 314, 250, PAPER, 18);
    text(ui.sports_dialog, "操场", 22, 12, &pet_font_20, INK);
    lv_obj_t *sports_close = button(ui.sports_dialog, 254, 2, 44, 44, TAG_CLOSE, PAPER, 12);
    lv_obj_t *sports_x = text(sports_close, "×", 0, 0, &pet_font_20, BLUE);
    lv_obj_center(sports_x);
    static const struct { const char *name; int cmd; } sports[] = {
        {"跑步", PW_RUN}, {"引体向上", PW_PULLUP}, {"仰卧起坐", PW_SITUP}};
    for (unsigned i = 0; i < 3; i++)
    {
        lv_obj_t *b = button(ui.sports_dialog, 22, 62 + (int)i * 54, 270, 44,
                             sports[i].cmd, 0xDCE6CB, 10);
        lv_obj_t *label = text(b, sports[i].name, 0, 0, &pet_font_16, INK);
        lv_obj_center(label);
    }
    ui.sports_game_dialog = box(ui.layer, 0, 0, 390, 450, 0x254D40, 0);
    ui.sports_run_layer = box(ui.sports_game_dialog, 0, 54, 390, 300, 0xB85C3C, 0);
    ui.sports_pull_layer = box(ui.sports_game_dialog, 0, 54, 390, 300, 0xBFD9D2, 0);
    ui.sports_sit_layer = box(ui.sports_game_dialog, 0, 54, 390, 300, 0xDCE6CB, 0);
    lv_obj_t *courts[] = {ui.sports_run_layer, ui.sports_pull_layer, ui.sports_sit_layer};
    for (unsigned i = 0; i < 3; i++)
    {
        image(courts[i], shahe_sports_courts[i], 0, 0);
        ui.sports_cat[i] = lv_image_create(courts[i]);
        lv_obj_remove_flag(ui.sports_cat[i], LV_OBJ_FLAG_CLICKABLE);
        set_image(ui.sports_cat[i], i == 0 ? pw_frames[12] : shahe_sports_poses[(i - 1) * 2]);
        lv_obj_set_pos(ui.sports_cat[i], 152, 129);
    }
    box(ui.sports_run_layer, 356, 184, 4, 74, 0xFFF1D6, 1);
    text(ui.sports_run_layer, "100m", 322, 262, &pet_font_16, 0xFFF1D6);
    box(ui.sports_pull_layer, 288, 84, 25, 180, 0x213E34, 10);
    box(ui.sports_pull_layer, 288, 145, 25, 59, 0x74C290, 8);
    ui.sports_marker = box(ui.sports_pull_layer, 291, 84, 19, 19, 0xFFF1D6, 10);
    ui.sports_score_label = text(ui.sports_game_dialog, "", 14, 4, &pet_font_20, 0xFFF1D6);
    ui.sports_timer = text(ui.sports_game_dialog, "", 14, 29, &pet_font_16, 0xF2E5C8);
    lv_obj_t *sports_game_close = button(ui.sports_game_dialog, 336, 6, 44, 44, TAG_SPORTS_CLOSE, 0x183932, 10);
    lv_obj_center(text(sports_game_close, "×", 0, 0, &pet_font_20, 0xFFF1D6));
    ui.sports_prompt = text(ui.sports_game_dialog, "", 16, 354, &pet_font_16, 0xFFF1D6);
    ui.sports_action = button(ui.sports_game_dialog, 45, 382, 132, 52, TAG_SPORTS_ACTION, 0xEEC16C, 16);
    ui.sports_right = button(ui.sports_game_dialog, 213, 382, 132, 52, TAG_SPORTS_RIGHT, 0x527566, 16);
    for (unsigned i = 0; i < 2; i++)
    {
        lv_obj_t *pad = i ? ui.sports_right : ui.sports_action;
        lv_obj_t *foot = box(pad, 0, 0, 20, 28, 0xFFF1D6, LV_RADIUS_CIRCLE);
        lv_obj_set_style_transform_pivot_x(foot, 10, 0);
        lv_obj_set_style_transform_pivot_y(foot, 14, 0);
        lv_obj_set_style_transform_rotation(foot, i ? 240 : -240, 0);
        lv_obj_center(foot);
        box(foot, 4, -4, 12, 8, 0xFFF1D6, LV_RADIUS_CIRCLE);
    }
    visible(ui.sports_game_dialog, false);

    ui.badminton_dialog = box(ui.layer, 0, 0, 390, 450, 0x2D574C, 0);
    lv_obj_add_flag(ui.badminton_dialog, LV_OBJ_FLAG_CLICKABLE);
    image(ui.badminton_dialog, &shahe_badminton_room, 0, 0);
    lv_obj_t *head = box(ui.badminton_dialog, 16, 8, 358, 66, 0x183932, 15);
    lv_obj_set_style_bg_opa(head, LV_OPA_90, 0);
    ui.badminton_score = text(ui.badminton_dialog, "", 0, 0, &pet_font_20, 0xFFF1D5);
    ui.badminton_timer = text(ui.badminton_dialog, "", 0, 0, &pet_font_16, 0xEADABC);
    lv_obj_t *court_close = button(ui.badminton_dialog, 318, 15, 48, 48, TAG_BADMINTON_CLOSE, 0x40665B, 10);
    lv_obj_center(text(court_close, "×", 0, 0, &pet_font_20, 0xFFF1D5));

    ui.badminton_zone = button(ui.badminton_dialog, 32, 135, 126, 134,
                               TAG_BADMINTON_HIT, 0xEEC16C, 13);
    lv_obj_set_style_border_width(ui.badminton_zone, 2, 0);
    lv_obj_set_style_border_color(ui.badminton_zone, lv_color_hex(0xEEC16C), 0);
    lv_obj_t *own_shadow = box(ui.badminton_dialog, 41, 325, 67, 11, 0x354138, 8);
    lv_obj_t *other_shadow = box(ui.badminton_dialog, 287, 325, 67, 11, 0x354138, 8);
    lv_obj_set_style_bg_opa(own_shadow, LV_OPA_40, 0);
    lv_obj_set_style_bg_opa(other_shadow, LV_OPA_40, 0);
    image(ui.badminton_dialog, pw_frames[12], 39, 259);
    lv_obj_t *rival = lv_image_create(ui.badminton_dialog);
    set_image(rival, pw_frames[4]);
    lv_obj_set_pos(rival, 282, 259);
    lv_obj_set_style_image_recolor(rival, lv_color_hex(0x596770), 0);
    lv_obj_set_style_image_recolor_opa(rival, LV_OPA_50, 0);
    lv_obj_remove_flag(rival, LV_OBJ_FLAG_CLICKABLE);
    ui.badminton_racket = badminton_racket(ui.badminton_dialog, 92, 261, 0xAA552F);
    ui.badminton_rival_racket = badminton_racket(ui.badminton_dialog, 268, 261, 0x306477);

    ui.badminton_shuttle = box(ui.badminton_dialog, 0, 0, 28, 18, 0, 0);
    lv_obj_set_style_bg_opa(ui.badminton_shuttle, LV_OPA_TRANSP, 0);
    box(ui.badminton_shuttle, 1, 2, 14, 3, 0xFFF4D9, 1);
    box(ui.badminton_shuttle, 2, 7, 16, 3, 0xFFF4D9, 1);
    box(ui.badminton_shuttle, 1, 12, 14, 3, 0xFFF4D9, 1);
    box(ui.badminton_shuttle, 15, 4, 11, 10, 0xF3BC68, 5);
    visible(ui.badminton_shuttle, false);

    box(ui.badminton_dialog, 16, 343, 358, 92, 0x183932, 15);
    ui.badminton_note = text(ui.badminton_dialog, "", 0, 0, &pet_font_16, 0xFFF1D5);
    ui.badminton_hit = button(ui.badminton_dialog, 46, 378, 298, 47, TAG_BADMINTON_HIT, 0x294E43, 12);
    ui.badminton_action = text(ui.badminton_hit, "挥拍", 0, 0, &pet_font_20, 0xFFF1D5);
    lv_obj_center(ui.badminton_action);
    ui.library_panel = box(ui.layer, 24, 38, 342, 374, PAPER, 18);
    text(ui.library_panel, "图书馆 · 借还书", 18, 12, &pet_font_20, INK);
    lv_obj_t *lib_close = button(ui.library_panel, 290, 2, 44, 44, TAG_CLOSE, PAPER, 12);
    lv_obj_t *lib_x = text(lib_close, "×", 0, 0, &pet_font_20, BLUE); lv_obj_center(lib_x);
    static const char *books[] = {"《沙河植物图鉴》", "《校园猫观察笔记》", "《嵌入式系统小史》",
                                 "《图书馆的猫》", "《北京风物》"};
    for (unsigned i = 0; i < 5; i++)
    {
        lv_obj_t *row = box(ui.library_panel, 16, 58 + (int)i * 56, 310, 50, 0xE6ECDE, 8);
        ui.library_book_name[i] = text(row, books[i], 10, 14, &pet_font_16, INK);
        ui.library_book_button[i] = button(row, 236, 3, 68, 44, TAG_BOOK_FIRST + (int)i, 0xDCEAF3, 8);
        lv_obj_t *bl = text(ui.library_book_button[i], "借阅", 0, 0, &pet_font_16, INK);
        lv_obj_center(bl);
    }
    text(ui.library_panel, "点击借阅/归还，试点5本", 18, 344, &pet_font_16, 0x62736B);
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
    for (unsigned i = 0; i < 4; i++)
    {
        ui.memory_tile[i] = button(ui.memory_dialog, 20 + (i % 2) * 152,
                                   111 + (i / 2) * 83, 140, 72, PW_MEMORY_0 + i, 0xE7E4CF, 13);
        ui.memory_symbol[i] = text(ui.memory_tile[i], "", 0, 0, &pet_font_20, INK);
        lv_obj_center(ui.memory_symbol[i]);
        ui.memory_icon[i] = new_game_icon(ui.memory_tile[i], 26);
    }
    ui.memory_footer = text(ui.memory_dialog, "", 20, 281, &pet_font_16, BLUE);
    ui.teaching_controls = box(ui.memory_dialog, 0, 0, 332, 312, PAPER, 0);
    lv_obj_set_style_bg_opa(ui.teaching_controls, LV_OPA_TRANSP, 0);
    ui.projection_pattern = box(ui.teaching_controls, 20, 111, 292, 40, 0xEAE6D6, 8);
    for (unsigned i = 0; i < PG_PROJECTION_STEPS; i++)
    {
        ui.projection_icon[i] = new_game_icon(ui.projection_pattern, 18);
        lv_obj_align(ui.projection_icon[i], LV_ALIGN_LEFT_MID, 15 + (int)i * 40, 0);
    }
    text(ui.projection_pattern, "?", 256, 6, &pet_font_20, INK);
    for (unsigned i = 0; i < PG_CIRCUIT_LINKS; i++)
    {
        ui.circuit_wire[i] = lv_line_create(ui.teaching_controls);
        lv_obj_set_style_line_width(ui.circuit_wire[i], 4, 0);
        lv_obj_set_style_line_color(ui.circuit_wire[i], lv_color_hex(game_colors[i]), 0);
    }
    static const lv_point_precise_t trace[] = {{117, 271}, {139, 271}, {147, 265},
        {157, 274}, {167, 265}, {177, 274}, {185, 271}, {213, 271}};
    ui.circuit_trace = lv_line_create(ui.teaching_controls);
    lv_line_set_points(ui.circuit_trace, trace, sizeof(trace) / sizeof(trace[0]));
    lv_obj_set_style_line_width(ui.circuit_trace, 2, 0);
    lv_obj_set_style_line_color(ui.circuit_trace, lv_color_hex(0x367467), 0);
    for (unsigned i = 0; i < PG_GRID_CELLS; i++)
    {
        ui.puzzle_tile[i] = button(ui.teaching_controls, 0, 0, 92, 44, PW_GRID_FIRST + (int)i, 0xDCEAF3, 8);
        ui.puzzle_icon[i] = new_game_icon(ui.puzzle_tile[i], 20);
    }
    visible(ui.teaching_controls, false);
    ui.challenge_track = box(ui.memory_dialog, 20, 126, 292, 20, 0xDCEAF3, 10);
    box(ui.challenge_track, 107, 0, 78, 20, 0x88AA92, 10);
    ui.challenge_cursor = box(ui.challenge_track, 0, 0, 5, 20, INK, 2);
    for (unsigned i = 0; i < 6; i++)
        ui.rhythm_beats[i] = box(ui.challenge_track, 12 + (int)i * 49, 5, 10, 10, 0xB4C4BE, LV_RADIUS_CIRCLE);
    for (unsigned i = 0; i < 4; i++)
    {
        ui.find_icon[i] = find_object(ui.memory_dialog, i);
        lv_obj_t *crop = box(ui.memory_tile[i], 0, 0, 64, 64, PAPER, 8);
        lv_obj_center(crop);
        ui.meal_icon[i] = crop;
        unsigned tile = challenge_dishes[i];
        image(crop, &pet_food_atlas, -(int)(tile % 3) * 64, -(int)(tile / 3) * 64);
        visible(crop, false);
        visible(ui.find_icon[i], false);
    }
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
static void voice_mic_event(lv_event_t *event)
{
    lv_event_code_t code = lv_event_get_code(event);
    if (code == LV_EVENT_PRESSED) pet_voice_hold(true);
    else if (code == LV_EVENT_RELEASED) pet_voice_hold(false);
    else if (code == LV_EVENT_PRESS_LOST)
    {
        pet_voice_hold(false);
        pet_voice_cancel();
    }
}

static void build_voice(lv_obj_t *screen)
{
    ui.voice_entry = button(screen, 24, 94, 76, 44, TAG_VOICE, HUD_PAPER, 8);
    lv_obj_set_style_bg_opa(ui.voice_entry, LV_OPA_90, 0);
    lv_obj_center(text(ui.voice_entry, "语音", 0, 0, &pet_font_16, HUD_INK));

    ui.voice_page = box(screen, 0, 0, 390, 450, 0x192B3C, 0);
    lv_obj_add_flag(ui.voice_page, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_t *back = button(ui.voice_page, 22, 23, 64, 44, TAG_VOICE_BACK, 0x2B4357, 12);
    lv_obj_center(text(back, "返回", 0, 0, &pet_font_16, 0xFCF1DA));
    lv_obj_align(text(ui.voice_page, "和小猫说话", 0, 0, &pet_font_20, 0xFCF1DA),
                 LV_ALIGN_TOP_MID, 0, 36);

    lv_obj_t *cat = lv_image_create(ui.voice_page);
    set_image(cat, pw_frames[0]);
    lv_obj_remove_flag(cat, LV_OBJ_FLAG_CLICKABLE);
    lv_image_set_pivot(cat, 36, 38);
    lv_image_set_scale(cat, 384);
    lv_obj_align(cat, LV_ALIGN_CENTER, 0, -12);

    const uint8_t heights[] = {4, 7, 11, 16, 11, 7, 4};
    for (unsigned i = 0; i < sizeof(heights); i++)
        ui.voice_wave[i] = box(ui.voice_page, 148 + 15 * i, 283 - heights[i] / 2,
            5, heights[i], 0xEAB07B, 3);

    ui.voice_status = text(ui.voice_page, "", 0, 0, &pet_font_20, 0xFCF1DA);
    ui.voice_detail = text(ui.voice_page, "", 0, 0, &pet_font_16, 0xD2E2E9);
    ui.voice_mic = button(ui.voice_page, 108, 374, 174, 52, 0, 0xEAB07B, 26);
    lv_obj_remove_event_cb(ui.voice_mic, click);
    lv_obj_add_event_cb(ui.voice_mic, voice_mic_event, LV_EVENT_ALL, NULL);
    lv_obj_center(text(ui.voice_mic, "按住说话", 0, 0, &pet_font_20, 0x192B3C));
    ui.voice_action = button(ui.voice_page, 108, 374, 174, 52, TAG_VOICE_ACTION, 0xEAB07B, 26);
    ui.voice_action_text = text(ui.voice_action, "", 0, 0, &pet_font_20, 0x192B3C);
    ui.voice_volume_panel = box(ui.voice_page, 322, 127, 48, 174, 0x2B4357, 20);
    lv_obj_set_style_bg_opa(ui.voice_volume_panel, LV_OPA_90, 0);
    text(ui.voice_volume_panel, "音量", 8, 8, &pet_font_16, 0xD2E2E9);
    for (unsigned i = 0; i < PV_VOLUME_COUNT; i++)
        ui.voice_volume_bar[i] = box(ui.voice_volume_panel, 13, 109 - 36 * (int)i, 22, 29, 0x567487, 9);
    ui.voice_volume_text = text(ui.voice_volume_panel, "", 0, 0, &pet_font_16, 0xFCF1DA);
    visible(ui.voice_volume_panel, false);
    visible(ui.voice_page, false);
}

static void paint_voice(void)
{
    visible(ui.voice_volume_panel, ui.world.now < ui.voice_volume_until);
    pet_voice_view_t v = pet_voice_view();
    static const char *titles[] = {
        "语音服务未连接", "等待手机共享网络", "核对手机配对码", "正在连接小智", "绑定小智设备",
        "我在听", "正在录音", "小猫在想", "小猫在说", "语音连接失败"
    };
    set_label(ui.voice_status, titles[v.state]);
    lv_obj_align(ui.voice_status, LV_ALIGN_TOP_MID, 0, 310);
    const char *detail = "";
    const char *action = "";
    switch (v.state)
    {
    case PV_NETWORK: detail = "蓝牙中选择 " PET_VOICE_BT_NAME; break;
    case PV_PAIR: detail = v.code; action = "配对码相同"; break;
    case PV_ACTIVATE: detail = v.code; action = "已绑定，重试"; break;
    case PV_RECORDING: detail = "松手发送"; break;
    case PV_WAITING:
    case PV_PLAYING: action = "停止"; break;
    case PV_ERROR: detail = "检查共享网络后重试"; action = "重试"; break;
    default: break;
    }
    set_label(ui.voice_detail, detail);
    lv_obj_align(ui.voice_detail, LV_ALIGN_TOP_MID, 0, 343);
    bool mic_visible = v.state == PV_READY || v.state == PV_RECORDING;
    visible(ui.voice_mic, mic_visible);
    visible(ui.voice_action, *action != 0);
    if (*action)
    {
        set_label(ui.voice_action_text, action);
        lv_obj_center(ui.voice_action_text);
    }
    for (unsigned i = 0; i < 7; i++)
    {
        unsigned distance = i > 3 ? i - 3 : 3 - i;
        unsigned height = 4 + (3 - distance) * 3;
        if (v.state == PV_RECORDING || v.state == PV_PLAYING)
            height += v.level * (4 - distance) / 4000;
        else if (v.state == PV_WAITING || v.state == PV_CONNECTING)
            height += (unsigned)((ui.world.now / 100 + i) % 7);
        lv_obj_set_height(ui.voice_wave[i], (int)height);
        lv_obj_set_y(ui.voice_wave[i], 283 - (int)height / 2);
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
    ui.library_shelf = button(screen, 18, 374, 112, 44, TAG_LIBRARY_SHELF, HUD_PAPER, 9);
    lv_obj_set_style_bg_opa(ui.library_shelf, LV_OPA_80, 0);
    lv_obj_t *shelf_label = text(ui.library_shelf, "借还书", 0, 0, &pet_font_16, INK);
    lv_obj_center(shelf_label);
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
    build_voice(screen);
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
static const char *room_title(const pet_world_t *w)
{
    if (w->scene == PW_CLASSROOM)
    {
        switch (w->building_id)
        {
        case PW_LIBRARY_BUILDING: return "图书馆 · 自习";
        case 3: return "公共教学楼 · 教室";
        case PW_LAB_BUILDING: return "教学实验综合楼·实验室";
        case 5: return "智慧教学楼 · 自习";
        case 10: return "科研楼 · 实验室";
        case 11: return "智能工程学院 · 实验室";
        case 12: return "计算机学院 · 机房";
        case 13: return "网络安全学院 · 机房";
        case 14: return "数字媒体学院 · 工作室";
        case 15: return "人文学院 · 教室";
        case 16: return "马克思主义学院 · 教室";
        case 17: return "理学院 · 实验室";
        case 18: return "工程实验楼 · 实验室";
        case 19: return "综合办公楼 · 办公室";
        default: return "教综楼 · 教室";
        }
    }
    if (w->scene == PW_CANTEEN)
        return w->building_id == 6 ? "风味餐厅 · 窗口" : w->building_id == 7 ? "教工餐厅 · 窗口" : "学生餐厅 · 食堂";
    return room_names[w->scene];
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
    int list_top = canteen ? 118 : 78;
    lv_obj_set_pos(ui.menu_list, 12, list_top);
    lv_obj_set_height(ui.menu_list, 320 - list_top);
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
    visible(ui.teaching_controls, false);
    visible(ui.challenge_track, false);
    for (unsigned i = 0; i < 4; i++)
    {
        visible(ui.memory_tile[i], true);
        lv_obj_set_pos(ui.memory_tile[i], 20 + (i % 2) * 152, 111 + (i / 2) * 83);
        lv_obj_set_size(ui.memory_tile[i], 140, 72);
        visible(ui.memory_symbol[i], false);
        visible(ui.memory_icon[i], true);
        game_icon(ui.memory_icon[i], i, game_colors[i]);
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
        set_label_fmt(ui.memory_footer, "已答 %u / %u · 剩余 %u 秒", game->input_at, game->length,
                      (pm_remaining(game) + 999) / 1000);
    else if (game->phase == PM_WON)
        set_label_fmt(ui.memory_footer, "金币 +%d · 心情 %+d", ui.world.coin_gain, ui.world.mood_gain);
    else if (game->phase == PM_LOST)
        set_label_fmt(ui.memory_footer, "参与奖励 +%d 金币", ui.world.coin_gain);
    else
        set_label(ui.memory_footer, game->phase == PM_SHOW ? "先记住顺序，再依次点击"
                                                                  : "随时可以重新挑战");
}
static void paint_teaching_game(const pg_game_t *g)
{
    bool projection = g->kind == PG_PROJECTION;
    bool circuit = g->kind == PG_CIRCUIT;
    bool won = g->phase == PG_WON, lost = g->phase == PG_LOST;
    bool active = g->phase == PG_INPUT;
    unsigned seconds = active ? (pg_remaining(g) + 999) / 1000 : 0;
    visible(ui.teaching_controls, true);
    visible(ui.challenge_track, false);
    visible(ui.projection_pattern, projection);
    set_label(ui.memory_title, projection ? "投影抢答" : circuit ? "修电路" : "互动屏校准");
    const char *instruction = projection ? "按规律选下一个" : circuit ? "点两个同色图案" : "本格和上下左右一起翻转";
    set_label(ui.memory_note, won ? (circuit ? "灯亮了，通电成功" : "挑战完成") :
                              lost ? "差一点，下次再试" : instruction);

    if (projection)
    {
        uint8_t pattern[PG_PROJECTION_STEPS], options[3];
        pg_projection(g, pattern, options);
        if (active)
            set_label_fmt(ui.memory_round, "第 %u / 3 题 · 对 %u · %u 秒", g->round + 1, g->score, seconds);
        else
            set_label_fmt(ui.memory_round, "答对 %u / 3", g->score);
        visible(ui.projection_pattern, active);
        for (unsigned i = 0; i < PG_PROJECTION_STEPS; i++)
            game_icon(ui.projection_icon[i], pattern[i], game_colors[pattern[i]]);
        for (unsigned i = 0; i < 3; i++)
        {
            lv_obj_set_pos(ui.memory_tile[i], 20 + (int)i * 100, 166);
            lv_obj_set_size(ui.memory_tile[i], 92, 74);
            visible(ui.memory_symbol[i], false);
            visible(ui.memory_icon[i], true);
            game_icon(ui.memory_icon[i], options[i], game_colors[options[i]]);
            lv_obj_set_style_bg_color(ui.memory_tile[i], lv_color_hex(0xF3DFB3), 0);
            lv_obj_set_style_border_width(ui.memory_tile[i], 0, 0);
        }
    }
    else if (circuit)
    {
        if (active)
            set_label_fmt(ui.memory_round, "接通 %u / 3 · 错 %u / 3 · %u 秒", g->score, g->mistakes, seconds);
        else
            set_label_fmt(ui.memory_round, "接通 %u / 3 · 错 %u / 3", g->score, g->mistakes);
    }
    else if (active)
        set_label_fmt(ui.memory_round, "剩余 %u 步 · %u 秒", PG_GRID_MOVES - g->round, seconds);
    else
        set_label_fmt(ui.memory_round, "已用 %u / %u 步", g->round, PG_GRID_MOVES);
    for (unsigned i = 0; i < 4; i++)
        visible(ui.memory_tile[i], projection && active && i < 3);

    for (unsigned i = 0; i < PG_CIRCUIT_LINKS; i++)
    {
        bool show = circuit && (g->links & (1U << i));
        visible(ui.circuit_wire[i], show);
        if (show)
        {
            unsigned right = 0;
            while (pg_circuit_color(g, right + PG_CIRCUIT_LINKS) != i) right++;
            ui.circuit_points[i][0] = (lv_point_precise_t){90, 130 + (int)i * 53};
            ui.circuit_points[i][1] = (lv_point_precise_t){242, 130 + (int)right * 53};
            lv_line_set_points(ui.circuit_wire[i], ui.circuit_points[i], 2);
        }
    }
    visible(ui.circuit_trace, circuit && won);
    for (unsigned i = 0; i < PG_GRID_CELLS; i++)
    {
        visible(ui.puzzle_tile[i], !projection && (!circuit || i < PG_CIRCUIT_PORTS));
        if (projection || (circuit && i >= PG_CIRCUIT_PORTS)) continue;
        lv_obj_t *tile = ui.puzzle_tile[i];
        uint32_t color;
        if (circuit)
        {
            unsigned match = pg_circuit_color(g, i);
            lv_obj_set_pos(tile, i < PG_CIRCUIT_LINKS ? 20 : 242, 108 + (int)(i % PG_CIRCUIT_LINKS) * 53);
            lv_obj_set_size(tile, 70, 44);
            game_icon(ui.puzzle_icon[i], match, game_colors[match]);
            color = g->selected == i ? 0xF4D382 : 0xEBECE2;
            lv_obj_set_style_border_width(tile, g->selected == i || (g->links & (1U << match)) ? 2 : 0, 0);
            lv_obj_set_style_border_color(tile, lv_color_hex(game_colors[match]), 0);
        }
        else
        {
            bool on = (g->grid & (1U << i)) != 0;
            lv_obj_set_pos(tile, 20 + (int)(i % 3) * 100, 106 + (int)(i / 3) * 54);
            lv_obj_set_size(tile, 92, 48);
            game_icon(ui.puzzle_icon[i], on ? 0 : 3, on ? 0x287A6A : 0x75818A);
            color = on ? 0x9EDDED : 0xDBE3EC;
            lv_obj_set_style_border_width(tile, 0, 0);
        }
        lv_obj_set_style_bg_color(tile, lv_color_hex(color), 0);
    }
    if (won) set_label_fmt(ui.memory_footer, "金币 +%d · 心情 %+d", ui.world.coin_gain, ui.world.mood_gain);
    else if (lost) set_label_fmt(ui.memory_footer, "参与奖励 +%d 金币", ui.world.coin_gain);
    else set_label(ui.memory_footer, projection ? "答对两题即过关" : circuit ? "接通三个图案" : "让九格全部点亮");
}

static void paint_challenge(void)
{
    const pg_game_t *g = &ui.world.challenge;
    if (g->kind >= PG_PROJECTION)
    {
        paint_teaching_game(g);
        return;
    }
    visible(ui.teaching_controls, false);
    bool rhythm = g->kind == PG_RHYTHM;
    bool won = g->phase == PG_WON, lost = g->phase == PG_LOST;
    static const char *objects[] = {"水杯", "书本", "钥匙", "耳机"};
    set_label(ui.memory_title, rhythm ? "健身节奏" : g->kind == PG_FIND ? "宿舍找物" : "食堂配餐");
    if (won || lost) set_label(ui.memory_note, won ? "答对啦，完成挑战" : "差一点，下次再试");
    else if (rhythm) set_label(ui.memory_note, pg_window(g) ? "绿色时间，点一下！" : "等指针走到绿色");
    else if (g->phase == PG_SHOW) set_label(ui.memory_note, "先记住物品的位置");
    else set_label_fmt(ui.memory_note, "%s：%s", g->kind == PG_FIND ? "找一找" : "订单", 
        g->kind == PG_FIND ? objects[g->targets[g->round]] : pe_menu(false, challenge_dishes[g->targets[g->round]])->name);
    if (rhythm) set_label_fmt(ui.memory_round, "第 %u / 6 拍 · 命中 %u", g->round < 6 ? g->round + 1 : 6, g->score);
    else if (won || lost) set_label_fmt(ui.memory_round, "完成 %u / 3", g->score);
    else set_label_fmt(ui.memory_round, "%u / 3 · 剩余 %u 秒", g->score, g->total < 30000 ? (30000 - g->total + 999) / 1000 : 0);
    visible(ui.challenge_track, rhythm);
    lv_obj_set_x(ui.challenge_cursor, (int)(g->elapsed % 3000 * 287U / 3000));
    for (unsigned i = 0; i < 6; i++)
        lv_obj_set_style_bg_color(ui.rhythm_beats[i], lv_color_hex(i < g->round ? 0x49685A : 0xB4C4BE), 0);
    for (unsigned i = 0; i < 4; i++)
    {
        visible(ui.memory_tile[i], !rhythm || i == 0);
        visible(ui.memory_symbol[i], false);
        visible(ui.meal_icon[i], g->kind == PG_MEAL);
        bool reveal = g->kind == PG_FIND && g->phase == PG_SHOW;
        visible(ui.find_icon[i], reveal);
        if (reveal)
        {
            unsigned slot = 0;
            while (g->cards[slot] != i) slot++;
            lv_obj_set_pos(ui.find_icon[i], 64 + (int)(slot % 2) * 152, 121 + (int)(slot / 2) * 83);
        }
        bool beat = rhythm && pg_window(g) && !g->pressed;
        visible(ui.memory_icon[i], !reveal && g->kind != PG_MEAL && (!rhythm || i == 0));
        game_icon(ui.memory_icon[i], beat ? 0 : 3, beat ? 0xF4CF75 : 0x51768C);
        lv_obj_set_pos(ui.memory_tile[i], rhythm ? 20 : 20 + (i % 2) * 152,
                       rhythm ? 165 : 111 + (i / 2) * 83);
        lv_obj_set_size(ui.memory_tile[i], rhythm ? 292 : 140, rhythm ? 88 : 72);
        lv_obj_set_style_bg_color(ui.memory_tile[i], lv_color_hex(beat ? 0x557F68 : 0xDCEAF3), 0);
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
    bool library = pw_in_library(&ui.world);
    unsigned building = ui.world.building_id;
    const lv_image_dsc_t *alternate = NULL;
    if (ui.world.scene == PW_CLASSROOM && building < sizeof(classroom_art) / sizeof(*classroom_art))
        alternate = classroom_art[building];
    if (ui.scene != ui.world.scene || ui.building_id != building)
    {
        ui.scene = ui.world.scene;
        ui.building_id = (uint8_t)building;
        if (!library && ui.panel == PANEL_LIBRARY)
            ui.panel = PANEL_NONE;
        if (ui.scene != PW_DORM && (ui.panel == PANEL_ORDER || ui.panel == PANEL_SHOP))
            ui.panel = PANEL_NONE;
        build_hotspots();
        if (ui.scene != PW_CAMPUS)
        {
            lv_obj_remove_event_cb(ui.blackboard, click);
            pw_command_t cmd = ui.scene == PW_DORM ? PW_FIND_GAME : ui.scene == PW_CANTEEN ? PW_MEAL_GAME
                : ui.scene == PW_GYM ? PW_RHYTHM_GAME : PW_MEMORY_START;
            lv_obj_add_event_cb(ui.blackboard, click, LV_EVENT_CLICKED, (void *)(uintptr_t)cmd);
            const char *game = ui.scene == PW_DORM ? "找物" : ui.scene == PW_CANTEEN ? "配餐"
                : ui.scene == PW_GYM ? "节奏" : "记忆题";
            if (ui.scene == PW_CLASSROOM && building == PW_PUBLIC_BUILDING) game = "投影抢答";
            if (ui.scene == PW_CLASSROOM && building == PW_SMART_BUILDING) game = "互动屏校准";
            set_label(ui.game_entry_label, game);
            bool room_game = ui.scene != PW_CLASSROOM;
            lv_obj_set_pos(ui.blackboard, room_game ? 143 : 147, room_game ? 96 : 108);
            lv_obj_set_size(ui.blackboard, room_game ? 104 : 145, room_game ? 44 : 71);
            lv_obj_set_style_bg_opa(ui.blackboard, room_game ? LV_OPA_80 : LV_OPA_TRANSP, 0);
            lv_obj_set_style_bg_opa(ui.game_entry_label, room_game ? LV_OPA_TRANSP : LV_OPA_80, 0);
            lv_obj_set_style_border_opa(ui.game_entry_label, room_game ? LV_OPA_TRANSP : LV_OPA_60, 0);
            lv_obj_align(ui.game_entry_label, room_game ? LV_ALIGN_CENTER : LV_ALIGN_TOP_RIGHT,
                         room_game ? 0 : -3, room_game ? 0 : 3);
            lv_obj_remove_event_cb(ui.context_button, click);
            lv_obj_add_event_cb(ui.context_button, click, LV_EVENT_CLICKED,
                (void *)(uintptr_t)(ui.scene == PW_CLASSROOM && building == PW_LAB_BUILDING ? PW_MEMORY_START : PW_STUDY));
            static const int life_pos[][2] = {{10, 346}, {303, 307}, {308, 296}, {0, 0}, {280, 290}};
            lv_obj_set_pos(ui.life_hit, life_pos[ui.scene][0], alternate ? 354 : life_pos[ui.scene][1]);
            lv_obj_set_height(ui.life_hit, 44);
            if (ui.scene == PW_CLASSROOM && building == PW_LAB_BUILDING)
                lv_obj_set_pos(ui.work_button, 202, 202);
            else
                lv_obj_set_pos(ui.work_button, 10, 120);
            lv_obj_set_pos(ui.door, library ? 307 : 310, library ? 142 : 100);
            lv_obj_set_size(ui.door, library ? 79 : 69, 143);
        }
        if (ui.scene != PW_CAMPUS)
        {
            const lv_image_dsc_t *background = alternate ? alternate : room_art[ui.scene];
            set_image(ui.background, library ? &shahe_library_room : background);
            const lv_image_dsc_t *art = room_decor[ui.scene];
            set_image(ui.decor, art);
            lv_obj_set_pos(ui.decor, decor_anchor[ui.scene][0], decor_anchor[ui.scene][1] - art->header.h);
        }
    }
    paint_clock();
    bool campus = ui.world.scene == PW_CAMPUS;
    if (library)
        lv_obj_set_pos(ui.context_button, 138, 374);
    else if (ui.world.scene == PW_CLASSROOM && building == PW_LAB_BUILDING)
        lv_obj_set_pos(ui.context_button, 23, 270);
    else
        lv_obj_set_pos(ui.context_button, 246, 196);
    lv_obj_set_y(ui.bubble_text, library ? -4 : -18);
    visible(ui.campus, campus);
    visible(ui.background, !campus);
    visible(ui.decor, !campus && !library && !alternate);
    for (unsigned i = 0; i < 3; i++)
        visible(ui.furniture[i], ui.world.scene == PW_DORM && (ui.world.wallet.equipped & (1U << i)));
    visible(ui.phone, ui.world.scene == PW_DORM);
    visible(ui.decor_entry, ui.world.scene == PW_DORM);
    bool room_game = ui.scene == PW_DORM || ui.scene == PW_CANTEEN || ui.scene == PW_GYM;
    visible(ui.blackboard, !campus && (room_game || ui.world.building_id == 1 ||
                                      ui.world.building_id == 3 || ui.world.building_id == 5));
    visible(ui.life_hit, !campus && !library);
    bool college_lab = ui.world.building_id == 10 || ui.world.building_id == 11 || ui.world.building_id == 12 ||
                       ui.world.building_id == 17 || ui.world.building_id == 18;
    visible(ui.work_button, ui.world.scene == PW_CLASSROOM && ui.world.phase != PW_MINIGAME &&
                            (ui.world.building_id == 1 || ui.world.building_id == 4 || college_lab));
    visible(ui.context_button, ui.world.scene == PW_CLASSROOM && ui.world.phase != PW_MINIGAME &&
                               (library || ui.world.building_id == 4 || ui.world.building_id >= 10));
    visible(ui.canteen_order, ui.world.scene == PW_CANTEEN && ui.world.phase != PW_MINIGAME);
    visible(ui.status_button, ui.world.phase != PW_MINIGAME);
    visible(ui.location, ui.world.phase != PW_MINIGAME);
    visible(ui.bag, ui.world.scene == PW_DORM && ui.world.order == PW_ORDER_READY &&
                    ui.world.action != PW_TAKEOUT &&
                    !(ui.world.phase == PW_WALK && ui.world.target == PW_TAKEOUT &&
                      ui.world.purpose != PW_WALK_TO_TAKEOUT_BAG));
    visible(ui.door, !campus);
    if (!campus)
        set_label(ui.location_text, room_title(&ui.world));
    for (unsigned i = 0; i < 5; i++)
    {
        lv_obj_t *b = lv_obj_get_child(ui.library_book_button[i], 0);
        set_label(b, ui.library_borrowed & (1U << i) ? "归还" : "借阅");
    }
    set_label(ui.context_label,
              building == PW_LAB_BUILDING ? "修电路"
              : (building == 10 || building == 11 || building == 17 || building == 18) ? "做实验"
              : (ui.world.building_id == 12 || ui.world.building_id == 13) ? "上机"
              : (library ? "看书" : "自习"));
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
    bool reading = library && ui.world.phase == PW_ACTION && ui.world.action == PW_STUDY;
    int anchor_y = reading ? 94 : 64;
    float sprite_y = ui.world.phase == PW_WALK ? v.y - v.lift : v.y;
    set_image(ui.sprite, reading ? &pw_library_reading : pw_frames[v.frame]);
    lv_image_set_pivot(ui.sprite, 36, anchor_y);
    lv_obj_set_pos(ui.sprite, (int)lroundf(v.x) - 36, (int)lroundf(sprite_y) - anchor_y);
    lv_image_set_scale(ui.sprite, (uint32_t)lroundf(v.scale * 256));
    lv_obj_set_pos(ui.cat_hit, limit((int)v.x - 32, 0, 326), limit((int)v.y - 64, 0, 382));
    int rx = v.sleeping ? 22 : 12;
    lv_obj_set_size(ui.shadow, rx * 2, v.sleeping ? 12 : 6);
    lv_obj_set_pos(ui.shadow, (int)v.x - rx, (int)v.y + (v.sleeping ? -16 : -2));
    lv_obj_set_style_bg_opa(ui.shadow, v.airborne ? LV_OPA_0 : LV_OPA_20, 0);
    bool bed = ui.world.phase == PW_SLEEP || ui.world.action == PW_REST || ui.world.action == PW_WAKE;
    bool show_front = !campus && !library && !alternate && (v.occlusion || (bed && v.y < 170));
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
    for (unsigned i = 0; i < 4; i++)
    {
        visible(ui.find_icon[i], false);
        visible(ui.meal_icon[i], false);
    }

    visible(ui.layer, memory || ui.panel != PANEL_NONE);
    if (memory || ui.panel != PANEL_NONE) lv_obj_move_foreground(ui.layer);
    visible(ui.status_dialog, !memory && ui.panel == PANEL_STATUS);
    visible(ui.order_dialog, !memory && ui.panel == PANEL_ORDER);
    visible(ui.shop_dialog, !memory && ui.panel == PANEL_SHOP);
    visible(ui.curfew_dialog, !memory && ui.panel == PANEL_CURFEW);
    visible(ui.album_dialog, !memory && ui.panel == PANEL_ALBUM);
    visible(ui.activity_dialog, !memory && ui.panel == PANEL_ACTIVITY);
    visible(ui.sports_dialog, !memory && ui.panel == PANEL_SPORTS);
    visible(ui.library_shelf, library && ui.world.phase != PW_MINIGAME);
    visible(ui.library_panel, !memory && ui.panel == PANEL_LIBRARY);
    visible(ui.sports_game_dialog, !memory && ui.panel == PANEL_SPORTS_GAME);
    visible(ui.badminton_dialog, !memory && ui.panel == PANEL_BADMINTON);
    if (ui.panel == PANEL_BADMINTON) paint_badminton();
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
    visible(ui.voice_entry, can_open_voice());
    visible(ui.voice_page, ui.voice_open);
    if (ui.voice_open)
    {
        lv_obj_move_foreground(ui.voice_page);
        paint_voice();
    }
}
void pet_ui_update(const pet_world_t *world)
{
    bool curfew = world->curfew_events != ui.world.curfew_events;
    if (ui.panel == PANEL_BADMINTON &&
        (curfew || world->scene != PW_CAMPUS || world->phase == PW_MINIGAME))
        badminton_close();
    if (ui.panel == PANEL_SPORTS_GAME &&
        (curfew || world->scene != PW_CAMPUS || world->phase == PW_MINIGAME))
    {
        ui.sports_active = ui.sports_finished = false;
        ui.panel = PANEL_NONE;
    }
    if (curfew) ui.panel = PANEL_CURFEW;
    else if (world->phase == PW_MINIGAME && ui.panel == PANEL_ALBUM) ui.panel = PANEL_NONE;
    else if (world->phase == PW_MINIGAME && ui.panel == PANEL_ACTIVITY) ui.panel = PANEL_NONE;
    else if (world->scene != PW_CAMPUS && ui.panel == PANEL_ACTIVITY) ui.panel = PANEL_NONE;
    else if (world->phase == PW_SEATED && ui.panel == PANEL_NONE && world->now - world->action_start >= 500)
    {
        ui.panel = PANEL_ORDER; ui.delivery = false;
        ui.menu_window = world->building_id == 6 ? 1 : world->building_id == 7 ? 2 : ui.pending_window;
        ui.selected = ui.menu_window ? (ui.menu_window - 1) * 2 : 0;
        ui.pending_window = 0;
        lv_obj_scroll_to_y(ui.menu_list, 0, LV_ANIM_OFF);
    }
    ui.world = *world;
    badminton_tick();
    sports_tick();
    paint();
}
