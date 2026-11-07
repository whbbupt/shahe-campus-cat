/* Electronic pet v0.6.1. T1 source and its verified branch remain in Git.
 * Display init follows SDK example/multimedia/lvgl/lvgl_v9_demos (Apache-2.0).
 */
#include "rtthread.h"
#include "littlevgl2rtt.h"
#include "pet_app.h"
#include "pet_ble.h"

#include "pet_version.h"
#define APP_VERSION PET_FIRMWARE_VERSION

int main(void)
{
    rt_kprintf("\n[pet] v%s boot ok\n", APP_VERSION);
    if (rt_device_find("lcd") == RT_NULL)
    {
        rt_kprintf("[pet] ERROR: lcd device not found\n");
        return -RT_ERROR;
    }
    rt_err_t result = littlevgl2rtt_init("lcd");
    if (result != RT_EOK || lv_display_get_default() == NULL)
    {
        rt_kprintf("[pet] ERROR: display init failed (%d)\n", result);
        return -RT_ERROR;
    }
    result = pet_app_start();
    if (result != RT_EOK)
        return result;
    result = pet_ble_start();
    if (result != RT_EOK)
        rt_kprintf("[pet:BLE] worker start failed=%d; pet continues\n", result);
    while (1)
    {
        pet_app_poll();
        uint32_t delay_ms = lv_timer_handler();
        if (delay_ms < 2) delay_ms = 2;
        if (delay_ms > 20) delay_ms = 20;
        rt_thread_mdelay(delay_ms);
    }
    return 0;
}
