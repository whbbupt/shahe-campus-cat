/*
 * course_starter 个人练习：屏幕启动文字 + 串口打印 + RGB 闪烁。
 *
 * 基于 SiFli-SDK 官方例程（Apache-2.0）：
 *   example/get-started/hello_world/rtt   —— 启动打印
 *   example/rt_device/rgb_led             —— RGB 灯驱动用法（SK6812MINI-HS，PA32，PWMT2 通道1）
 *   example/multimedia/lvgl/lvgl_v9_demos  —— LCD/LVGL 初始化与刷新入口
 * 板型：sf32lb52-lchspi-ulp（立创·黄山派）。
 *
 * 行为：屏幕和串口显示同一版本与练习文字；RGB 仍按红→绿→蓝亮/灭各 500ms。
 *       RGB 使用 LVGL 定时器，所有图形操作均在主线程；不再重复打印颜色日志。
 *
 * 新增 .c 文件直接放进 src/ 即可，src/SConscript 会自动收集。
 */
#include "rtthread.h"
#include "bf0_hal.h"
#include "drv_io.h"
#include "stdio.h"
#include "string.h"
#include "drivers/rt_drv_pwm.h"
#include "littlevgl2rtt.h"
#include "course_ui.h"

#define APP_NAME        "course_starter"
#define APP_VERSION     "0.1.2"
#define PRACTICE_MESSAGE "Week 03 practice: modified firmware is running."

#define RGBLED_NAME     "rgbled"
#define BLINK_ON_MS     500
#define BLINK_OFF_MS    500

struct rgb_color
{
    const char *name;
    uint32_t    value;      /* 0xRRGGBB */
};

static const struct rgb_color color_table[] =
{
    {"red",   0xff0000},
    {"green", 0x00ff00},
    {"blue",  0x0000ff},
};

static struct rt_device *rgbled_device;

static void rgb_led_init(void)
{
#ifdef SF32LB52X
    HAL_PIN_Set(PAD_PA32, GPTIM2_CH1, PIN_NOPULL, 1);   /* pwmt2_cc1 */
    /* RGB 灯供电 */
    HAL_PMU_ConfigPeriLdo(PMU_PERI_LDO3_3V3, true, true);
#endif
    rgbled_device = rt_device_find(RGBLED_NAME);
    if (!rgbled_device)
    {
        rt_kprintf("[%s] ERROR: device '%s' not found, check proj.conf RGB options\n", APP_NAME, RGBLED_NAME);
        RT_ASSERT(0);
    }
}

static void rgb_led_set_color(uint32_t color)
{
    struct rt_rgbled_configuration configuration;
    configuration.color_rgb = color;
    rt_device_control(rgbled_device, PWM_CMD_SET_COLOR, &configuration);
}

/* 原有亮/灭顺序由 UI 线程定时驱动，避免长延时阻塞 LVGL 刷新。 */
static void rgb_led_timer(lv_timer_t *timer)
{
    static unsigned int color_index = 0;
    static rt_bool_t is_on = RT_TRUE;

    if (is_on)
    {
        rgb_led_set_color(0x000000);
        is_on = RT_FALSE;
        lv_timer_set_period(timer, BLINK_OFF_MS);
    }
    else
    {
        color_index = (color_index + 1) % (sizeof(color_table) / sizeof(color_table[0]));
        rgb_led_set_color(color_table[color_index].value);
        is_on = RT_TRUE;
        lv_timer_set_period(timer, BLINK_ON_MS);
    }
}

int main(void)
{
    rt_kprintf("\n[%s] v%s boot ok\n", APP_NAME, APP_VERSION);
    rt_kprintf("%s\n", PRACTICE_MESSAGE);

    if (rt_device_find("lcd") == RT_NULL)
    {
        rt_kprintf("[%s] ERROR: lcd device not found\n", APP_NAME);
        return -RT_ERROR;
    }

    rt_err_t result = littlevgl2rtt_init("lcd");
    if (result != RT_EOK || lv_display_get_default() == NULL)
    {
        rt_kprintf("[%s] ERROR: display init failed (%d)\n", APP_NAME, result);
        return -RT_ERROR;
    }

    course_ui_create(APP_NAME, APP_VERSION, PRACTICE_MESSAGE);
    rt_kprintf("[%s] UI created (%dx%d), awaiting visual check\n", APP_NAME,
               (int)lv_display_get_horizontal_resolution(NULL),
               (int)lv_display_get_vertical_resolution(NULL));

    rgb_led_init();
    rgb_led_set_color(color_table[0].value);
    if (lv_timer_create(rgb_led_timer, BLINK_ON_MS, NULL) == NULL)
    {
        rgb_led_set_color(0x000000);
        rt_kprintf("[%s] ERROR: RGB timer allocation failed\n", APP_NAME);
        return -RT_ERROR;
    }

    while (1)
    {
        uint32_t delay_ms = lv_timer_handler();
        if (delay_ms < 2) delay_ms = 2;
        if (delay_ms > 20) delay_ms = 20;
        rt_thread_mdelay(delay_ms);
    }
    return 0;
}
