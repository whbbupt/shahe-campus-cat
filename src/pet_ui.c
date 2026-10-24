/* Original editable cat artwork, with small expression overlays in ROM. */
#include "pet_ui.h"
#include "pet_cat_assets.h"
#include "lvgl.h"
#include <stdint.h>

#define CLOUD 0xEAF3F8
#define INK   0x233B50
#define LAKE  0x2A7A89
#define MINT  0x80BDA6
#define SUN   0xFFC96B
#define MUTED 0x667D8B

static struct
{
    lv_timer_t *timer;
    lv_obj_t *cat, *face, *heart, *sleep_mark;
    lv_obj_t *food_value, *mood_value, *food_bar, *mood_bar, *state, *message;
    lv_obj_t *feed, *touch, *rest, *rest_text;
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

static lv_obj_t *button(int x, const char *text, pet_event_t event, bool primary)
{
    lv_obj_t *obj = lv_button_create(lv_screen_active());
    lv_obj_remove_style_all(obj);
    lv_obj_set_pos(obj, x, 347);
    lv_obj_set_size(obj, 108, 56);
    lv_obj_set_style_radius(obj, 16, 0);
    lv_obj_set_style_bg_color(obj, lv_color_hex(primary ? LAKE : 0xFFFFFF), 0);
    lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, 0);
    lv_obj_set_style_shadow_width(obj, 0, 0);
    lv_obj_set_style_border_width(obj, primary ? 0 : 2, 0);
    lv_obj_set_style_border_color(obj, lv_color_hex(LAKE), 0);
    /* Explicit styles keep disabled contrast stable without theme transitions. */
    lv_obj_set_style_opa(obj, LV_OPA_COVER, LV_STATE_DISABLED);
    lv_obj_set_style_bg_color(obj, lv_color_hex(0xD6E1E7), LV_STATE_DISABLED);
    lv_obj_set_style_text_color(obj, lv_color_hex(primary ? 0xFFFFFF : LAKE), 0);
    lv_obj_set_style_text_color(obj, lv_color_hex(0x506671), LV_STATE_DISABLED);
    lv_obj_add_event_cb(obj, clicked, LV_EVENT_CLICKED, (void *)(uintptr_t)event);
    lv_obj_t *text_obj = lv_label_create(obj);
    lv_label_set_text(text_obj, text);
    lv_obj_set_style_text_font(text_obj, &lv_font_montserrat_20, 0);
    lv_obj_center(text_obj);
    return obj;
}

static void set_enabled(lv_obj_t *obj, bool enabled)
{
    if (enabled)
        lv_obj_remove_state(obj, LV_STATE_DISABLED);
    else
        lv_obj_add_state(obj, LV_STATE_DISABLED);
}

static void animate(lv_timer_t *timer)
{
    (void)timer;
    pet_view_t view = pet_model_view(&ui.snapshot);
    ++ui.frame;
    bool sleep = view == PET_SLEEPING;
    bool blink = sleep || ui.frame % 40U >= 38U;
    const lv_image_dsc_t *face = &pet_cat_idle;
    if (blink) face = &pet_cat_blink;
    else if (view == PET_FEEDING) face = &pet_cat_eat;
    else if (view == PET_PETTING || view == PET_HAPPY) face = &pet_cat_happy;
    else if (view == PET_HUNGRY || view == PET_LONELY) face = &pet_cat_sad;
    if (lv_image_get_src(ui.face) != face)
        lv_image_set_src(ui.face, face);
    lv_obj_set_y(ui.cat, 136 + (sleep ? 2 : (ui.frame / 5U % 2U ? 1 : 0)));
    lv_obj_set_y(ui.heart, 210 - (int)(ui.frame % 8U / 2U));
    if (view == PET_PETTING)
        lv_obj_remove_flag(ui.heart, LV_OBJ_FLAG_HIDDEN);
    else
        lv_obj_add_flag(ui.heart, LV_OBJ_FLAG_HIDDEN);
    if (sleep)
        lv_obj_remove_flag(ui.sleep_mark, LV_OBJ_FLAG_HIDDEN);
    else
        lv_obj_add_flag(ui.sleep_mark, LV_OBJ_FLAG_HIDDEN);
}

void pet_ui_create(pet_ui_action_fn action)
{
    if (ui.timer)
    {
        lv_timer_delete(ui.timer);
        ui.timer = NULL;
    }
    lv_obj_t *screen = lv_screen_active();
    lv_obj_clean(screen);
    lv_obj_remove_flag(screen, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(screen, lv_color_hex(CLOUD), 0);
    ui.action = action;
    ui.frame = 0;

    label(screen, "MOMO", 24, 18, &lv_font_montserrat_28, INK);
    label(screen, "Your little companion", 24, 52, &lv_font_montserrat_16, MUTED);
    ui.state = label(screen, "HELLO", 250, 28, &lv_font_montserrat_16, LAKE);
    lv_obj_set_width(ui.state, 116);
    lv_obj_set_style_text_align(ui.state, LV_TEXT_ALIGN_RIGHT, 0);

    label(screen, "FOOD", 24, 85, &lv_font_montserrat_16, INK);
    label(screen, "MOOD", 204, 85, &lv_font_montserrat_16, INK);
    ui.food_value = label(screen, "70", 151, 85, &lv_font_montserrat_16, LAKE);
    ui.mood_value = label(screen, "60", 331, 85, &lv_font_montserrat_16, LAKE);
    shape(screen, 24, 109, 162, 8, 0xD0DEE5, 4);
    shape(screen, 204, 109, 162, 8, 0xD0DEE5, 4);
    ui.food_bar = shape(screen, 24, 109, 113, 8, LAKE, 4);
    ui.mood_bar = shape(screen, 204, 109, 97, 8, MINT, 4);

    lv_obj_t *room = shape(screen, 24, 136, 342, 162, 0xD7E8EE, 38);
    shape(room, 28, 144, 286, 4, 0xBDD1DB, 2);
    shape(room, 258, 22, 27, 27, SUN, LV_RADIUS_CIRCLE);
    ui.sleep_mark = label(screen, "z z", 277, 207, &lv_font_montserrat_24, LAKE);
    lv_obj_add_flag(ui.sleep_mark, LV_OBJ_FLAG_HIDDEN);

    ui.cat = shape(screen, 97, 136, 196, 160, CLOUD, 0);
    lv_obj_set_style_bg_opa(ui.cat, LV_OPA_TRANSP, 0);
    lv_obj_t *body = lv_image_create(ui.cat);
    lv_image_set_src(body, &pet_cat_base);
    lv_obj_remove_flag(body, LV_OBJ_FLAG_CLICKABLE);
    ui.face = lv_image_create(ui.cat);
    lv_obj_set_pos(ui.face, 56, 54);
    lv_image_set_src(ui.face, &pet_cat_idle);
    lv_obj_remove_flag(ui.face, LV_OBJ_FLAG_CLICKABLE);
    ui.heart = lv_image_create(screen);
    lv_image_set_src(ui.heart, &pet_cat_heart);
    lv_obj_set_pos(ui.heart, 278, 210);
    lv_obj_remove_flag(ui.heart, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(ui.heart, LV_OBJ_FLAG_HIDDEN);

    ui.message = label(screen, "Ready for a little care.", 24, 313, &lv_font_montserrat_16, INK);
    lv_obj_set_width(ui.message, 342);
    lv_label_set_long_mode(ui.message, LV_LABEL_LONG_CLIP);
    lv_obj_set_style_text_align(ui.message, LV_TEXT_ALIGN_CENTER, 0);
    ui.feed = button(24, "Feed", PET_FEED, true);
    ui.touch = button(141, "Pet", PET_TOUCH, false);
    ui.rest = button(258, "Rest", PET_REST, false);
    ui.rest_text = lv_obj_get_child(ui.rest, 0);
    label(screen, "Progress resets on restart", 24, 419, &lv_font_montserrat_16, MUTED);
    ui.timer = lv_timer_create(animate, 100, NULL);
}

void pet_ui_update(const pet_model_t *pet, const char *message)
{
    static const char *states[] = {"CONTENT", "HAPPY", "HUNGRY", "LONELY", "YUM!", "PURR!", "RESTING"};
    static const char *hints[] = {"Ready for a little care.", "A very happy little cat.", "A snack would be lovely.",
        "A little company, please.", "A delicious little break.", "That feels nice!", "Resting. Tap Wake to return."};
    ui.snapshot = *pet;
    pet_view_t view = pet_model_view(pet);
    lv_label_set_text_fmt(ui.food_value, "%u", (unsigned)pet->food);
    lv_label_set_text_fmt(ui.mood_value, "%u", (unsigned)pet->mood);
    /* Hide zero bars rather than asking LVGL to lay out a zero-width object. */
    lv_obj_set_width(ui.food_bar, pet->food ? (162 * pet->food + 99) / 100 : 1);
    lv_obj_set_width(ui.mood_bar, pet->mood ? (162 * pet->mood + 99) / 100 : 1);
    lv_obj_set_style_bg_opa(ui.food_bar, pet->food ? LV_OPA_COVER : LV_OPA_TRANSP, 0);
    lv_obj_set_style_bg_opa(ui.mood_bar, pet->mood ? LV_OPA_COVER : LV_OPA_TRANSP, 0);
    lv_label_set_text(ui.state, states[view]);
    lv_label_set_text(ui.message, message ? message : hints[view]);
    lv_label_set_text(ui.rest_text, pet->sleeping ? "Wake" : "Rest");
    set_enabled(ui.feed, !pet->sleeping && !pet->action_ms);
    set_enabled(ui.touch, !pet->sleeping && !pet->action_ms);
    set_enabled(ui.rest, !pet->action_ms);
    animate(NULL);
}
