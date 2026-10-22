/*
 * 第三周屏幕文字练习。使用 SDK 自带 LVGL v9 与 Montserrat 英文字体。
 * 只创建静态对象；没有 PASS 标识，也不以创建成功代替实物显示验证。
 */
#include "lvgl.h"
#include "course_ui.h"

void course_ui_create(const char *app_name, const char *version, const char *message)
{
    lv_obj_t *screen = lv_screen_active();
    lv_obj_clean(screen);
    lv_obj_remove_flag(screen, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(screen, lv_color_hex(0xF4F8FE), 0);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);

    lv_obj_t *name = lv_label_create(screen);
    lv_label_set_text(name, app_name);
    lv_obj_set_style_text_font(name, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(name, lv_color_hex(0x4B6581), 0);
    lv_obj_set_pos(name, 40, 72);

    lv_obj_t *version_label = lv_label_create(screen);
    lv_label_set_text_fmt(version_label, "v%s", version);
    lv_obj_set_style_text_font(version_label, &lv_font_montserrat_28, 0);
    lv_obj_set_style_text_color(version_label, lv_color_hex(0x16324F), 0);
    lv_obj_set_pos(version_label, 40, 112);

    lv_obj_t *accent = lv_obj_create(screen);
    lv_obj_remove_style_all(accent);
    lv_obj_set_style_bg_color(accent, lv_color_hex(0x2866BD), 0);
    lv_obj_set_style_bg_opa(accent, LV_OPA_COVER, 0);
    lv_obj_set_size(accent, 48, 4);
    lv_obj_set_pos(accent, 40, 162);

    lv_obj_t *text = lv_label_create(screen);
    lv_label_set_text(text, message);
    lv_label_set_long_mode(text, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(text, lv_display_get_horizontal_resolution(NULL) - 80);
    lv_obj_set_style_text_font(text, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(text, lv_color_hex(0x16324F), 0);
    lv_obj_set_style_text_line_space(text, 6, 0);
    lv_obj_set_pos(text, 40, 198);
}
