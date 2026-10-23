/* Uses production UI and real LVGL; input/time/framebuffer are simulated. */
#include "lvgl.h"
#include "pet_ui.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/* Report assertions to the captured log instead of a Windows CRT dialog. */
#undef assert
#define assert(condition) do { if (!(condition)) { \
    fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #condition); exit(1); \
} } while (0)

static pet_model_t model;
static unsigned char frame[450][390][3];
static unsigned char draw_buffer[390 * 50 * 3];

static void flush(lv_display_t *display, const lv_area_t *area, uint8_t *data)
{
    int width = area->x2 - area->x1 + 1;
    for (int y = area->y1; y <= area->y2; ++y)
        for (int x = area->x1; x <= area->x2; ++x)
        {
            assert(x >= 0 && x < 390 && y >= 0 && y < 450);
            unsigned offset = ((y - area->y1) * width + x - area->x1) * 3;
            frame[y][x][0] = data[offset + 2];
            frame[y][x][1] = data[offset + 1];
            frame[y][x][2] = data[offset];
        }
    lv_display_flush_ready(display);
}

static void clicked(pet_event_t event)
{
    pet_result_t result = pet_model_event(&model, event);
    assert(result == PET_OK);
    pet_ui_update(&model, NULL);
}

static lv_obj_t *find_button(const char *name)
{
    lv_obj_t *screen = lv_screen_active();
    for (unsigned i = 0; i < lv_obj_get_child_count(screen); ++i)
    {
        lv_obj_t *obj = lv_obj_get_child(screen, i);
        if (lv_obj_check_type(obj, &lv_button_class) &&
            !strcmp(lv_label_get_text(lv_obj_get_child(obj, 0)), name))
            return obj;
    }
    assert(!"button not found");
    return NULL;
}

static unsigned timer_count(void)
{
    unsigned count = 0;
    for (lv_timer_t *timer = lv_timer_get_next(NULL); timer; timer = lv_timer_get_next(timer))
        ++count;
    return count;
}

static void screenshot(const char *name)
{
    lv_obj_update_layout(lv_screen_active());
    for (unsigned i = 0; i < lv_obj_get_child_count(lv_screen_active()); ++i)
    {
        lv_area_t bounds;
        lv_obj_get_coords(lv_obj_get_child(lv_screen_active(), i), &bounds);
        assert(bounds.x1 >= 0 && bounds.y1 >= 0 && bounds.x2 < 390 && bounds.y2 < 450);
    }
    lv_refr_now(NULL);
    FILE *file = fopen(name, "wb");
    assert(file);
    fprintf(file, "P6\n390 450\n255\n");
    assert(fwrite(frame, sizeof(frame), 1, file) == 1);
    fclose(file);
}

int main(void)
{
    lv_init();
    lv_display_t *display = lv_display_create(390, 450);
    lv_display_set_color_format(display, LV_COLOR_FORMAT_RGB888);
    lv_display_set_buffers(display, draw_buffer, NULL, sizeof(draw_buffer), LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_flush_cb(display, flush);
    pet_model_init(&model);
    pet_ui_create(clicked);
    pet_ui_update(&model, NULL);
    screenshot("pet-idle.ppm");
    unsigned timers = timer_count();

    lv_obj_send_event(find_button("Feed"), LV_EVENT_CLICKED, NULL);
    assert(model.food == 85 && pet_model_view(&model) == PET_FEEDING);
    screenshot("pet-feeding.ppm");
    pet_model_advance(&model, 2000);
    pet_ui_update(&model, NULL);
    lv_obj_send_event(find_button("Rest"), LV_EVENT_CLICKED, NULL);
    assert(model.sleeping && lv_obj_has_state(find_button("Feed"), LV_STATE_DISABLED));
    screenshot("pet-resting.ppm");
    lv_obj_send_event(find_button("Wake"), LV_EVENT_CLICKED, NULL);
    assert(!model.sleeping && !lv_obj_has_state(find_button("Feed"), LV_STATE_DISABLED));
    for (unsigned i = 0; i < 10; ++i)
    {
        pet_ui_create(clicked);
        pet_ui_update(&model, NULL);
        lv_tick_inc(100);
        lv_timer_handler();
        assert(timer_count() == timers);
        assert(model.food == 85);
    }
    lv_obj_send_event(find_button("Pet"), LV_EVENT_CLICKED, NULL);
    assert(model.mood == 70 && pet_model_view(&model) == PET_PETTING);
    screenshot("pet-petting.ppm");
    puts("[host:ui] feed/rest/wake/pet and 10 recreations PASS; hardware not tested");
    return 0;
}
