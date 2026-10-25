/* Model-driven Shahe campus scenes; original image_gen art stays in ROM. */
#include "pet_ui.h"
#include "pet_cat_assets.h"
#include "pet_scene_assets.h"
#include "lvgl.h"
#include <stdint.h>

#define PAPER 0xF3F6F3
#define INK   0x30423E
#define BLUE  0x466F94
#define MIST  0xE0EAE6
#define MUTED 0x62736B

static struct
{
    lv_timer_t *timer;
    lv_obj_t *room, *background, *cat, *sprite, *heart, *sleep_mark, *scene_name;
    lv_obj_t *food_value, *mood_value, *food_bar, *mood_bar, *state, *message;
    lv_obj_t *study, *feed, *rest, *rest_text, *nav[3], *lesson, *lesson_fill;
    pet_ui_action_fn action;
    pet_model_t snapshot;
    unsigned frame;
} ui;

static lv_obj_t *shape(lv_obj_t *parent, int x, int y, int w, int h, uint32_t color, int radius)
{
    lv_obj_t *obj = lv_obj_create(parent);
    lv_obj_remove_style_all(obj);
    lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_pos(obj, x, y);
    lv_obj_set_size(obj, w, h);
    lv_obj_set_style_radius(obj, radius, 0);
    lv_obj_set_style_bg_color(obj, lv_color_hex(color), 0);
    lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, 0);
    return obj;
}

static lv_obj_t *label(lv_obj_t *parent, const char *text, int x, int y, const lv_font_t *font, uint32_t color)
{
    lv_obj_t *obj = lv_label_create(parent);
    lv_label_set_text(obj, text);
    lv_obj_set_pos(obj, x, y);
    lv_obj_set_style_text_font(obj, font, 0);
    lv_obj_set_style_text_color(obj, lv_color_hex(color), 0);
    return obj;
}

static void clicked(lv_event_t *event)
{
    if (ui.action)
        ui.action((pet_event_t)(uintptr_t)lv_event_get_user_data(event));
}

static lv_obj_t *button(int x, int y, const char *text, pet_event_t event, bool navigation)
{
    lv_obj_t *obj = lv_button_create(lv_screen_active());
    lv_obj_remove_style_all(obj);
    lv_obj_set_pos(obj, x, y);
    lv_obj_set_size(obj, 112, 44);
    lv_obj_set_style_radius(obj, navigation ? 16 : 14, 0);
    lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(obj, lv_color_hex(navigation ? PAPER : 0xFFFFFF), 0);
    lv_obj_set_style_border_width(obj, navigation ? 0 : 1, 0);
    lv_obj_set_style_border_color(obj, lv_color_hex(MIST), 0);
    lv_obj_set_style_text_color(obj, lv_color_hex(MUTED), 0);
    lv_obj_set_style_bg_color(obj, lv_color_hex(0xDFE8F0), LV_STATE_CHECKED);
    lv_obj_set_style_text_color(obj, lv_color_hex(BLUE), LV_STATE_CHECKED);
    lv_obj_set_style_bg_color(obj, lv_color_hex(0xD5E0E8), LV_STATE_PRESSED);
    lv_obj_set_style_text_color(obj, lv_color_hex(INK), LV_STATE_PRESSED);
    lv_obj_set_style_bg_color(obj, lv_color_hex(MIST), LV_STATE_DISABLED);
    lv_obj_set_style_text_color(obj, lv_color_hex(0x63716B), LV_STATE_DISABLED);
    lv_obj_set_style_opa(obj, LV_OPA_COVER, LV_STATE_DISABLED);
    lv_obj_set_style_outline_width(obj, 2, LV_STATE_FOCUSED);
    lv_obj_set_style_outline_color(obj, lv_color_hex(BLUE), LV_STATE_FOCUSED);
    lv_obj_add_event_cb(obj, clicked, LV_EVENT_CLICKED, (void *)(uintptr_t)event);
    lv_obj_t *text_obj = lv_label_create(obj);
    lv_label_set_text(text_obj, text);
    lv_obj_set_style_text_font(text_obj, navigation ? &lv_font_montserrat_16 : &lv_font_montserrat_20, 0);
    lv_obj_center(text_obj);
    return obj;
}

static void set_enabled(lv_obj_t *obj, bool enabled)
{
    if (enabled) lv_obj_remove_state(obj, LV_STATE_DISABLED);
    else lv_obj_add_state(obj, LV_STATE_DISABLED);
}

static void primary(lv_obj_t *obj, bool active)
{
    lv_obj_set_style_bg_color(obj, lv_color_hex(active ? BLUE : 0xFFFFFF), 0);
    lv_obj_set_style_text_color(obj, lv_color_hex(active ? 0xFFFFFF : INK), 0);
    lv_obj_set_style_border_width(obj, active ? 0 : 1, 0);
}

static void animate(lv_timer_t *timer)
{
    (void)timer;
    pet_view_t view = pet_model_view(&ui.snapshot);
    ++ui.frame;
    bool sleep = view == PET_SLEEPING;
    const lv_image_dsc_t *sprite = &pet_cat_idle;
    if (sleep || ui.frame % 40U >= 38U) sprite = &pet_cat_closed;
    else if (view == PET_FEEDING) sprite = &pet_cat_eat;
    else if (view == PET_PETTING || view == PET_HAPPY) sprite = &pet_cat_closed;
    else if (view == PET_HUNGRY || view == PET_LONELY) sprite = &pet_cat_sad;
    if (lv_image_get_src(ui.sprite) != sprite) lv_image_set_src(ui.sprite, sprite);
    lv_obj_set_y(ui.cat, 70 + (sleep ? 2 : (ui.frame / 5U % 2U ? 1 : 0)));
    lv_obj_set_y(ui.heart, 96 - (int)(ui.frame % 8U / 2U));
    if (view == PET_PETTING) lv_obj_remove_flag(ui.heart, LV_OBJ_FLAG_HIDDEN);
    else lv_obj_add_flag(ui.heart, LV_OBJ_FLAG_HIDDEN);
    if (sleep) lv_obj_remove_flag(ui.sleep_mark, LV_OBJ_FLAG_HIDDEN);
    else lv_obj_add_flag(ui.sleep_mark, LV_OBJ_FLAG_HIDDEN);
    if (view == PET_STUDYING)
    {
        uint32_t elapsed = ui.snapshot.action_ms <= 5000U ? 5000U - ui.snapshot.action_ms : 0;
        lv_obj_set_width(ui.lesson_fill, elapsed ? (int)(elapsed * 334U / 5000U) : 1);
        lv_obj_remove_flag(ui.lesson, LV_OBJ_FLAG_HIDDEN);
    }
    else lv_obj_add_flag(ui.lesson, LV_OBJ_FLAG_HIDDEN);
}

void pet_ui_create(pet_ui_action_fn action)
{
    if (ui.timer) { lv_timer_delete(ui.timer); ui.timer = NULL; }
    lv_obj_t *screen = lv_screen_active();
    lv_obj_clean(screen);
    lv_obj_remove_flag(screen, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(screen, lv_color_hex(PAPER), 0);
    ui.action = action;
    ui.frame = 0;

    label(screen, "MOMO", 20, 12, &lv_font_montserrat_28, INK);
    label(screen, "BUPT cat diary", 20, 44, &lv_font_montserrat_16, MUTED);
    ui.state = label(screen, "Settled in", 238, 20, &lv_font_montserrat_16, BLUE);
    lv_obj_set_width(ui.state, 132);
    lv_obj_set_style_text_align(ui.state, LV_TEXT_ALIGN_RIGHT, 0);
    lv_obj_t *campus = label(screen, "Shahe campus", 238, 44, &lv_font_montserrat_16, MUTED);
    lv_obj_set_width(campus, 132);
    lv_obj_set_style_text_align(campus, LV_TEXT_ALIGN_RIGHT, 0);

    label(screen, "Food", 20, 75, &lv_font_montserrat_16, INK);
    label(screen, "Mood", 206, 75, &lv_font_montserrat_16, INK);
    ui.food_value = label(screen, "70", 140, 75, &lv_font_montserrat_16, INK);
    ui.mood_value = label(screen, "60", 326, 75, &lv_font_montserrat_16, INK);
    lv_obj_set_width(ui.food_value, 44); lv_obj_set_width(ui.mood_value, 44);
    lv_obj_set_style_text_align(ui.food_value, LV_TEXT_ALIGN_RIGHT, 0);
    lv_obj_set_style_text_align(ui.mood_value, LV_TEXT_ALIGN_RIGHT, 0);
    shape(screen, 20, 99, 164, 4, MIST, 2);
    shape(screen, 206, 99, 164, 4, MIST, 2);
    ui.food_bar = shape(screen, 20, 99, 115, 4, 0xC89766, 2);
    ui.mood_bar = shape(screen, 206, 99, 98, 4, 0x83A793, 2);

    ui.room = shape(screen, 18, 109, 354, 216, MIST, 22);
    lv_obj_set_style_clip_corner(ui.room, true, 0);
    ui.background = lv_image_create(ui.room);
    lv_image_set_src(ui.background, &pet_scene_dorm);
    lv_obj_remove_flag(ui.background, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_t *shadow = shape(ui.room, 222, 162, 72, 7, INK, LV_RADIUS_CIRCLE);
    lv_obj_set_style_bg_opa(shadow, LV_OPA_20, 0);
    ui.cat = shape(ui.room, 202, 70, 112, 96, PAPER, 14);
    lv_obj_set_style_bg_opa(ui.cat, LV_OPA_TRANSP, 0);
    lv_obj_add_flag(ui.cat, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(ui.cat, clicked, LV_EVENT_CLICKED, (void *)(uintptr_t)PET_TOUCH);
    ui.sprite = lv_image_create(ui.cat);
    lv_image_set_src(ui.sprite, &pet_cat_idle);
    lv_obj_remove_flag(ui.sprite, LV_OBJ_FLAG_CLICKABLE);
    ui.heart = lv_image_create(ui.room);
    lv_image_set_src(ui.heart, &pet_cat_heart);
    lv_obj_set_pos(ui.heart, 315, 96);
    lv_obj_remove_flag(ui.heart, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(ui.heart, LV_OBJ_FLAG_HIDDEN);
    ui.sleep_mark = label(ui.room, "z z", 310, 111, &lv_font_montserrat_20, BLUE);
    lv_obj_add_flag(ui.sleep_mark, LV_OBJ_FLAG_HIDDEN);
    lv_obj_t *scene_chip = shape(ui.room, 12, 12, 112, 28, 0xFFFFFF, 10);
    lv_obj_set_style_bg_opa(scene_chip, LV_OPA_90, 0);
    ui.scene_name = label(scene_chip, "Dormitory", 0, 0, &lv_font_montserrat_16, INK);
    lv_obj_center(ui.scene_name);
    ui.lesson = shape(ui.room, 10, 179, 334, 3, MIST, 1);
    ui.lesson_fill = shape(ui.lesson, 0, 0, 1, 3, BLUE, 1);
    lv_obj_add_flag(ui.lesson, LV_OBJ_FLAG_HIDDEN);
    lv_obj_t *caption = shape(ui.room, 10, 183, 334, 28, 0xFFFFFF, 10);
    lv_obj_set_style_bg_opa(caption, LV_OPA_90, 0);
    ui.message = label(caption, "A quiet moment before class.", 10, 4, &lv_font_montserrat_16, INK);
    lv_obj_set_width(ui.message, 314);
    lv_label_set_long_mode(ui.message, LV_LABEL_LONG_CLIP);
    lv_obj_set_style_text_align(ui.message, LV_TEXT_ALIGN_CENTER, 0);

    ui.nav[0] = button(18, 330, "Dorm", PET_GO_DORM, true);
    ui.nav[1] = button(139, 330, "Class", PET_GO_CLASSROOM, true);
    ui.nav[2] = button(260, 330, "Canteen", PET_GO_CANTEEN, true);
    ui.study = button(18, 382, "Study", PET_STUDY, false);
    ui.feed = button(139, 382, "Eat", PET_FEED, false);
    ui.rest = button(260, 382, "Rest", PET_REST, false);
    ui.rest_text = lv_obj_get_child(ui.rest, 0);
    label(screen, "Progress resets on restart", 20, 430, &lv_font_montserrat_16, MUTED);
    ui.timer = lv_timer_create(animate, 100, NULL);
}

void pet_ui_update(const pet_model_t *pet, const char *message)
{
    static const lv_image_dsc_t *backgrounds[] = {&pet_scene_dorm, &pet_scene_classroom, &pet_scene_canteen};
    static const char *places[] = {"Dormitory", "Classroom", "Canteen"};
    static const char *idle_hints[] = {"Tap Momo for a little pat.", "Ready for a little learning?", "Something warm for lunch?"};
    static const char *states[] = {"Settled in", "Happy", "A little hungry", "Needs a pat", "Meal time", "Purr!", "Resting", "In class"};
    static const char *hints[] = {NULL, "A happy little campus day.", "A meal would help.", "Tap Momo for a little pat.",
        "Enjoying a warm meal.", "Purr... that feels nice.", "Resting. Tap Wake to return.", "Learning, step by step."};
    ui.snapshot = *pet;
    pet_view_t view = pet_model_view(pet);
    if (lv_image_get_src(ui.background) != backgrounds[pet->scene]) lv_image_set_src(ui.background, backgrounds[pet->scene]);
    lv_label_set_text(ui.scene_name, places[pet->scene]); lv_obj_center(ui.scene_name);
    lv_label_set_text_fmt(ui.food_value, "%u", (unsigned)pet->food);
    lv_label_set_text_fmt(ui.mood_value, "%u", (unsigned)pet->mood);
    lv_obj_set_width(ui.food_bar, pet->food ? (164 * pet->food + 99) / 100 : 1);
    lv_obj_set_width(ui.mood_bar, pet->mood ? (164 * pet->mood + 99) / 100 : 1);
    lv_obj_set_style_bg_opa(ui.food_bar, pet->food ? LV_OPA_COVER : LV_OPA_TRANSP, 0);
    lv_obj_set_style_bg_opa(ui.mood_bar, pet->mood ? LV_OPA_COVER : LV_OPA_TRANSP, 0);
    lv_label_set_text(ui.state, states[view]);
    lv_label_set_text(ui.message, message ? message : (view == PET_IDLE ? idle_hints[pet->scene] : hints[view]));
    lv_label_set_text(ui.rest_text, pet->sleeping ? "Wake" : "Rest");
    lv_obj_center(ui.rest_text);
    bool available = !pet->sleeping && !pet->action_ms;
    for (unsigned i = 0; i < 3; ++i)
    {
        set_enabled(ui.nav[i], available);
        if ((unsigned)pet->scene == i) lv_obj_add_state(ui.nav[i], LV_STATE_CHECKED);
        else lv_obj_remove_state(ui.nav[i], LV_STATE_CHECKED);
    }
    set_enabled(ui.study, available); set_enabled(ui.feed, available);
    set_enabled(ui.rest, !pet->action_ms); set_enabled(ui.cat, available);
    primary(ui.study, pet->scene == PET_CLASSROOM);
    primary(ui.feed, pet->scene == PET_CANTEEN);
    primary(ui.rest, pet->scene == PET_DORM);
    animate(NULL);
}
