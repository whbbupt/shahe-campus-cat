/* Room views and continuous campus camera. Model owns movement and settlement. */
#include "pet_ui.h"
#include "lvgl.h"
#include "pet_version.h"
#include "pet_world_assets.h"
#include "pet_world_font.h"
#include "src/core/lv_obj_event_private.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

#define INK 0x30423E
#define BLUE 0x466F94
#define PAPER 0xFAFCF7
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
    lv_obj_t *campus, *door, *location, *location_text, *corner_name, *goal;
    pet_world_t world;
    int panel, scene;
    lv_obj_t *background, *shadow, *sprite, *cat_hit, *foreground, *marks, *hotspot[2];
    lv_obj_t *layer, *dialog, *status, *travel, *food, *mood, *food_fill, *mood_fill, *state, *title,
        *dest[3], *dest_note[3];
    lv_obj_t *corner[2], *bubble, *bubble_text, *save_note;
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
static void road_click(lv_event_t *e)
{
    (void)e;
    if (ui.panel || ui.world.scene != PW_CAMPUS || !ui.point)
        return;
    lv_indev_t *indev = lv_indev_active();
    if (!indev)
        return;
    lv_point_t p;
    lv_indev_get_point(indev, &p);
    float x, y;
    pw_camera(&ui.world, &x, &y);
    ui.point(p.x + x, p.y + y);
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
    if (cmd == TAG_TRAVEL && ui.world.scene != PW_CAMPUS)
    {
        if (ui.action)
            ui.action(PW_EXIT);
        return;
    }
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
    if (ui.panel && cmd < PW_GO_DORM)
        return;
    pw_result_t result = ui.action ? ui.action((pw_command_t)cmd) : PW_INVALID;
    if (cmd >= PW_GO_DORM && cmd <= PW_GO_CANTEEN && result == PW_OK)
    {
        ui.panel = 0;
        paint();
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
    lv_image_set_src(o, src);
    lv_obj_set_pos(o, x, y);
    lv_obj_remove_flag(o, LV_OBJ_FLAG_CLICKABLE);
}
static void tree(lv_obj_t *p, int x, int y, int size)
{
    lv_obj_t *shade = box(p, x + 8, y + size - 7, size, 14, 0xA9BC91, LV_RADIUS_CIRCLE);
    lv_obj_set_style_bg_opa(shade, LV_OPA_50, 0);
    box(p, x + size / 2 - 4, y + size / 2, 8, size / 2 + 9, 0x957C54, 2);
    box(p, x, y + 6, size, size - 8, 0x6E916A, LV_RADIUS_CIRCLE);
    box(p, x + 5, y, size - 9, size - 12, 0x8FAA79, LV_RADIUS_CIRCLE);
}
static void building(lv_obj_t *p, int x, int y, int w, int h, int room, const char *name)
{
    box(p, x + 8, y + 9, w, h, 0xAEBC98, 7);
    box(p, x, y, w, h, 0xEAD9B7, 5);
    box(p, x - 6, y - 6, w + 12, 32, room == PW_CANTEEN ? 0x819E86 : 0x738C99, 5);
    box(p, x - 6, y + 20, w + 12, 5, 0x506E78, 0);
    for (int row = 0; row < (h > 120 ? 2 : 0); row++)
        for (int col = 0; col < 3; col++)
        {
            int wx = x + 18 + col * (w - 36) / 3, wy = y + 42 + row * 36;
            box(p, wx, wy, 30, 24, 0xA9C5C6, 2);
            box(p, wx + 14, wy, 2, 24, 0xF6EDDA, 0);
        }
    lv_obj_t *sign = box(p, x + w / 2 - 45, y + h - 72, 90, 26, PAPER, 4);
    lv_obj_t *label = text(sign, name, 0, 0, &pet_font_16, INK);
    lv_obj_center(label);
    box(p, x + w / 2 - 19, y + h - 43, 38, 43, 0x567C86, 3);
    box(p, x + w / 2 - 1, y + h - 40, 2, 37, 0xB5D3D1, 0);
    lv_obj_t *door = button(p, x + 10, y + h - 90, w - 20, 102, PW_ENTER_DORM + room, PAPER, 3);
    lv_obj_set_style_bg_opa(door, LV_OPA_TRANSP, 0);
    lv_obj_set_style_bg_opa(door, LV_OPA_20, LV_STATE_PRESSED);
    text(p, "进入", x + w / 2 - 16, y + h + 13, &pet_font_16, BLUE);
}
static void build_campus(lv_obj_t *screen)
{
    ui.campus = box(screen, 0, 0, 1120, 1120, 0xC5D8AC, 0);
    lv_obj_add_flag(ui.campus, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(ui.campus, road_click, LV_EVENT_CLICKED, NULL);
    box(ui.campus, 220, 650, 250, 260, 0xB7CEA0, 50);
    box(ui.campus, 670, 650, 230, 260, 0xB7CEA0, 50);
    static lv_point_precise_t road_points[24][2];
    for (int pass = 0; pass < 2; pass++)
        for (unsigned i = 0; i < pw_road_count(); i++)
        {
            pw_point_t a, b;
            pw_road(i, &a, &b);
            road_points[i][0] = (lv_point_precise_t){a.x, a.y};
            road_points[i][1] = (lv_point_precise_t){b.x, b.y};
            {
                lv_obj_t *line = lv_line_create(ui.campus);
                lv_line_set_points(line, road_points[i], 2);
                lv_obj_set_style_line_width(line, pass ? 64 : 76, 0);
                lv_obj_set_style_line_color(line, lv_color_hex(pass ? 0xE2D5BA : 0xC1BCA1), 0);
                lv_obj_set_style_line_rounded(line, true, 0);
                lv_obj_remove_flag(line, LV_OBJ_FLAG_CLICKABLE);
            }
        }
    /* Paving marks encode distance along the main avenues. */
    for (int k = 240; k < 930; k += 80)
    {
        box(ui.campus, k, 536, 2, 48, 0xD2C4A9, 0);
        box(ui.campus, 536, k, 48, 2, 0xD2C4A9, 0);
    }
    box(ui.campus, 520, 520, 80, 80, 0xEBDFC7, 20);
    building(ui.campus, 270, 270, 180, 190, PW_DORM, "宿舍");
    building(ui.campus, 470, 20, 180, 100, PW_CLASSROOM, "教学楼");
    building(ui.campus, 670, 270, 180, 190, PW_CANTEEN, "食堂");
    static const int16_t trees[][2] = {
        {60, 65},    {260, 60},    {370, 66},   {730, 65},   {845, 55},   {1030, 80},  {60, 280},
        {55, 440},   {70, 650},    {65, 800},   {70, 1010},  {260, 1030}, {410, 1030}, {690, 1030},
        {825, 1020}, {1035, 1010}, {1035, 830}, {1035, 660}, {1030, 440}, {1035, 280}, {455, 265},
        {655, 265},  {270, 690},   {380, 830},  {710, 820},  {825, 680},  {425, 650},  {650, 650}};
    for (unsigned i = 0; i < sizeof(trees) / sizeof(trees[0]); i++)
        tree(ui.campus, trees[i][0], trees[i][1], 45 + (i % 3) * 5);
    /* Two shaded benches beside the southern loop. */
    for (int i = 0; i < 2; i++)
    {
        int x = 375 + i * 290;
        box(ui.campus, x, 710, 70, 10, 0x8B7856, 2);
        box(ui.campus, x, 728, 70, 14, 0xAC9064, 3);
        box(ui.campus, x + 8, 742, 5, 8, INK, 0);
        box(ui.campus, x + 56, 742, 5, 8, INK, 0);
    }
    box(ui.campus, 610, 465, 7, 65, 0x8D795B, 2);
    lv_obj_t *sign = box(ui.campus, 599, 456, 110, 30, PAPER, 5);
    text(sign, "↑ 教学楼", 8, 3, &pet_font_16, INK);
    sign = box(ui.campus, 599, 489, 110, 30, 0x8BA295, 5);
    text(sign, "食堂 →", 8, 3, &pet_font_16, PAPER);
    text(ui.campus, "宿舍 ←", 390, 607, &pet_font_16, INK);
    text(ui.campus, "草坪环路", 595, 835, &pet_font_20, INK);
    ui.goal = box(ui.campus, 0, 0, 20, 20, BLUE, LV_RADIUS_CIRCLE);
    lv_obj_set_style_bg_opa(ui.goal, LV_OPA_20, 0);
    lv_obj_set_style_border_width(ui.goal, 2, 0);
    lv_obj_set_style_border_color(ui.goal, lv_color_hex(BLUE), 0);
    visible(ui.campus, false);
}
static const int16_t zones[3][2][6][2] = {
    {{{0, 146}, {72, 91}, {150, 104}, {121, 250}, {72, 305}, {0, 256}},
     {{250, 140}, {310, 127}, {371, 173}, {370, 256}, {320, 280}, {264, 247}}},
    {{{61, 242}, {137, 208}, {198, 246}, {178, 316}, {94, 373}, {51, 321}}, {{0}}},
    {{{24, 269}, {93, 226}, {163, 281}, {144, 355}, {58, 390}, {22, 337}}, {{0}}}};
static const int16_t marker[3][2][2] = {{{43, 247}, {327, 154}}, {{143, 249}, {0, 0}}, {{110, 277}, {0, 0}}};
static bool in_polygon(int x, int y, unsigned target)
{
    bool hit = false;
    for (int i = 0, j = 5; i < 6; j = i++)
    {
        int ax = zones[ui.world.scene][target][i][0], ay = zones[ui.world.scene][target][i][1],
            bx = zones[ui.world.scene][target][j][0], by = zones[ui.world.scene][target][j][1];
        if ((ay > y) != (by > y) && x < (float)(bx - ax) * (y - ay) / (by - ay) + ax)
            hit = !hit;
    }
    int dx = x - marker[ui.world.scene][target][0], dy = y - marker[ui.world.scene][target][1];
    return hit || dx * dx + dy * dy <= 22 * 22;
}
static void hit_test(lv_event_t *e)
{
    lv_hit_test_info_t *h = lv_event_get_param(e);
    unsigned target = (unsigned)(uintptr_t)lv_event_get_user_data(e);
    if (h && h->res)
        h->res = in_polygon(h->point->x, h->point->y, target);
}
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
    for (unsigned t = 0; t < (ui.world.scene == PW_DORM ? 2U : 1U); t++)
    {
        int minx = 390, miny = 450, maxx = 0, maxy = 0;
        for (int n = 0; n < 6; n++)
        {
            int x = zones[ui.world.scene][t][n][0], y = zones[ui.world.scene][t][n][1];
            if (x < minx)
                minx = x;
            if (x > maxx)
                maxx = x;
            if (y < miny)
                miny = y;
            if (y > maxy)
                maxy = y;
        }
        int mx = marker[ui.world.scene][t][0], my = marker[ui.world.scene][t][1];
        if (mx - 22 < minx)
            minx = mx - 22;
        if (my - 22 < miny)
            miny = my - 22;
        if (mx + 22 > maxx)
            maxx = mx + 22;
        if (my + 22 > maxy)
            maxy = my + 22;
        if (minx < 0)
            minx = 0;
        if (miny < 0)
            miny = 0;
        if (maxx > 389)
            maxx = 389;
        if (maxy > 449)
            maxy = 449;
        pw_command_t cmd = ui.world.scene == PW_DORM ? (t ? PW_CODE : PW_REST)
                                                     : (ui.world.scene == PW_CLASSROOM ? PW_STUDY : PW_EAT);
        lv_obj_t *o = button(lv_screen_active(), minx, miny, maxx - minx + 1, maxy - miny + 1, cmd, PAPER, 0);
        ui.hotspot[t] = o;
        lv_obj_set_style_bg_opa(o, LV_OPA_TRANSP, 0);
        lv_obj_set_style_bg_opa(o, LV_OPA_TRANSP, LV_STATE_PRESSED);
        lv_obj_add_flag(o, LV_OBJ_FLAG_ADV_HITTEST);
        lv_obj_add_event_cb(o, hit_test, LV_EVENT_HIT_TEST, (void *)(uintptr_t)t);
        lv_obj_t *disc = box(o, mx - minx - 12, my - miny - 12, 24, 24, PAPER, 12);
        lv_obj_set_style_bg_opa(disc, LV_OPA_90, 0);
        image(disc, pw_icons[cmd == PW_REST ? 2 : cmd == PW_CODE ? 3 : cmd == PW_STUDY ? 4 : 5], 2, 2);
        lv_obj_set_size(lv_obj_get_child(disc, 0), 20, 20);
        lv_image_set_scale(lv_obj_get_child(disc, 0), 213);
        lv_obj_move_to_index(o, 1 + t);
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
    text(ui.travel, "教学楼  ↑  北侧", 8, 6, &pet_font_20, INK);
    text(ui.travel, "宿舍  ←     →  食堂", 8, 49, &pet_font_20, INK);
    text(ui.travel, "草坪环路  ↓  南侧", 8, 92, &pet_font_20, INK);
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
    lv_obj_remove_flag(ui.background, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_pos(ui.background, 0, 0);
    lv_image_set_src(ui.background, pw_backgrounds[0]);
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
    lv_image_set_src(ui.foreground, pw_occluders[0]);
    ui.door = button(screen, 344, 284, 44, 44, PW_EXIT, PAPER, 0);
    lv_obj_set_style_bg_opa(ui.door, LV_OPA_TRANSP, 0);
    lv_obj_set_style_bg_opa(ui.door, LV_OPA_TRANSP, LV_STATE_PRESSED);
    ui.location = box(screen, 13, 13, 174, 52, PAPER, 14);
    lv_obj_set_style_bg_opa(ui.location, LV_OPA_90, 0);
    text(ui.location, "北邮 · 沙河", 12, 2, &pet_font_16, BLUE);
    ui.location_text = text(ui.location, "十字路口", 12, 24, &pet_font_16, INK);
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
        lv_obj_t *name = text(ui.corner[i], i ? "出门" : "状态", 10, 29, &pet_font_16, BLUE);
        if (i)
            ui.corner_name = name;
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
        {
            lv_image_set_src(ui.background, pw_backgrounds[ui.scene]);
            lv_image_set_src(ui.foreground, pw_occluders[ui.scene]);
            lv_obj_set_pos(ui.foreground, pw_occluder_bounds[ui.scene][0], pw_occluder_bounds[ui.scene][1]);
            pw_point_t door = pw_node(ui.world.scene, 0);
            lv_obj_set_pos(ui.door, 344, (int)door.y - 16);
        }
    }
    bool campus = ui.world.scene == PW_CAMPUS;
    visible(ui.campus, campus);
    visible(ui.location, campus);
    visible(ui.background, !campus);
    visible(ui.door, !campus);
    lv_label_set_text(ui.corner_name, campus ? "路标" : "出门");
    float cx, cy;
    pw_camera(&ui.world, &cx, &cy);
    lv_obj_set_pos(ui.campus, -(int)lroundf(cx), -(int)lroundf(cy));
    if (campus)
    {
        const char *place =
            ui.world.y < 280
                ? "教学楼前"
                : (ui.world.y > 700
                       ? "草坪环路"
                       : (ui.world.x < 450 ? "宿舍小路" : (ui.world.x > 670 ? "食堂小路" : "十字路口")));
        lv_label_set_text(ui.location_text, place);
    }
    visible(ui.goal, campus && ui.world.phase == PW_WALK && ui.world.purpose == 3);
    lv_obj_set_pos(ui.goal, (int)ui.world.point_goal.x - 10, (int)ui.world.point_goal.y - 10);
    pw_visual_t v;
    pw_visual(&ui.world, &v);
    v.x -= cx;
    v.y -= cy;
    lv_image_set_src(ui.sprite, pw_frames[v.frame]);
    lv_obj_set_pos(ui.sprite, (int)lroundf(v.x) - 36, (int)lroundf(v.y) - 64);
    lv_image_set_scale(ui.sprite, (uint32_t)lroundf(v.scale * 256));
    lv_obj_set_pos(ui.cat_hit, limit((int)v.x - 32, 0, 326), limit((int)v.y - 64, 0, 382));
    int rx = v.sleeping ? 22 : 12;
    lv_obj_set_size(ui.shadow, rx * 2, v.sleeping ? 12 : 6);
    lv_obj_set_pos(ui.shadow, (int)v.x - rx, (int)v.y + (v.sleeping ? -16 : -2));
    lv_obj_set_style_bg_opa(ui.shadow, v.airborne ? LV_OPA_0 : LV_OPA_20, 0);
    visible(ui.foreground, !campus && v.occlusion);
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
        msg = "v" PET_FIRMWARE_VERSION " · 校园探索";
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
