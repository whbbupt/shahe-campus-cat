#ifndef COURSE_UI_H
#define COURSE_UI_H

/* 调用前需完成 LVGL 显示初始化；由 UI 主线程调用一次。 */
void course_ui_create(const char *app_name, const char *version, const char *message);

#endif
