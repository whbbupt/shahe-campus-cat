/*
 * course_starter T1练习：屏幕启动文字 + 按键切换RGB。
 *
 * 基于 SiFli-SDK 官方例程（Apache-2.0）：
 *   example/get-started/hello_world/rtt   —— 启动打印
 *   example/rt_device/rgb_led             —— RGB 灯驱动用法（SK6812MINI-HS，PA32，PWMT2 通道1）
 *   example/multimedia/lvgl/lvgl_v9_demos  —— LCD/LVGL 初始化与刷新入口
 * 板型：sf32lb52-lchspi-ulp（立创·黄山派）。
 *
 * 行为：初始红灯，按下KEY1后按红→绿→蓝切换；不再自动轮换。
 *       按键线程只发原始边沿事件，RGB与LVGL都由主线程操作。
 *       第一版按训练要求不消抖，见ADR-0001。
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
#include "key.h"

#define APP_NAME        "course_starter"
#define APP_VERSION     "0.2.0"
#define PRACTICE_MESSAGE "T1 practice: press KEY1 to change RGB color."

#define RGBLED_NAME     "rgbled"

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

static rt_err_t rgb_led_set_color(uint32_t color)
{
    struct rt_rgbled_configuration configuration;
    configuration.color_rgb = color;
    return rt_device_control(rgbled_device, PWM_CMD_SET_COLOR, &configuration);
}

static void process_key_events(void)
{
    /* Bounded batch lets LVGL run even if the raw input produces many edges. */
    for (unsigned int i = 0; i < T1_KEY_QUEUE_DEPTH; i++)
    {
        struct t1_key_event event;
        rt_err_t result = t1_key_receive(&event);
        if (result != RT_EOK)
        {
            if (result != -RT_ETIMEOUT)
                rt_kprintf("[t1] ERROR: key queue receive failed (%d)\n", result);
            break;
        }
        if (event.color_index >= sizeof(color_table) / sizeof(color_table[0]))
        {
            rt_kprintf("[t1] ERROR: invalid color index %lu\n", (unsigned long)event.color_index);
            continue;
        }
        result = rgb_led_set_color(color_table[event.color_index].value);
        if (result != RT_EOK)
        {
            rt_kprintf("[t1] ERROR: RGB command for press %lu failed (%d)\n",
                       (unsigned long)event.press_count, result);
            continue;
        }
        rt_kprintf("[t1] press=%lu color=%s\n", (unsigned long)event.press_count,
                   color_table[event.color_index].name);
    }
}

int main(void)
{
    rt_kprintf("\n[%s] v%s boot ok\n", APP_NAME, APP_VERSION);
    rt_kprintf("%s\n", PRACTICE_MESSAGE);
    if (!t1_key_self_test())
        return -RT_ERROR;

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
    result = rgb_led_set_color(color_table[0].value);
    if (result != RT_EOK)
    {
        rt_kprintf("[t1] ERROR: initial RGB command failed (%d)\n", result);
        return result;
    }
    rt_kprintf("[t1] initial color=red\n");
    result = t1_key_start();
    if (result != RT_EOK)
    {
        rt_kprintf("[t1] ERROR: key start failed (%d)\n", result);
        return result;
    }
    rt_kprintf("[t1] ready: KEY1 pin=%d active-high poll=%dms; no debounce\n",
               BSP_KEY1_PIN, T1_KEY_POLL_MS);

    while (1)
    {
        process_key_events();
        uint32_t delay_ms = lv_timer_handler();
        if (delay_ms < 2) delay_ms = 2;
        if (delay_ms > 20) delay_ms = 20;
        rt_thread_mdelay(delay_ms);
    }
    return 0;
}
