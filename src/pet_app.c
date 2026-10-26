#include "pet_app.h"
#include "pet_model.h"
#include "pet_ui.h"
#include "pet_world.h"
static pet_world_t world;
static rt_tick_t previous_tick;
static uint32_t fraction, display_elapsed;
static uint32_t sync_clock(void)
{
    rt_tick_t now = rt_tick_get();
    uint32_t ms = pet_clock_elapsed(previous_tick, now, RT_TICK_PER_SECOND, &fraction);
    previous_tick = now;
    pw_advance(&world, ms);
    return ms;
}
static pw_result_t command(pw_command_t cmd)
{
    sync_clock();
    pw_result_t result = pw_request(&world, cmd);
    pet_ui_update(&world);
    rt_kprintf("[pet] cmd=%u result=%u scene=%u phase=%u food=%u mood=%u\n", (unsigned)cmd, (unsigned)result,
               (unsigned)world.scene, (unsigned)world.phase, (unsigned)world.food, (unsigned)world.mood);
    return result;
}
static void report(const char *name, bool ok)
{
    rt_kprintf("[pet:WORLD] %s %s\n", name, ok ? "PASS" : "FAIL");
}
rt_err_t pet_app_start(void)
{
    if (!pw_selftest(report))
    {
        rt_kprintf("[pet:WORLD] all FAIL; UI not started\n");
        return -RT_ERROR;
    }
    rt_kprintf("[pet:WORLD] all PASS\n");
    pw_init(&world);
    previous_tick = rt_tick_get();
    fraction = display_elapsed = 0;
    pet_ui_create(command);
    pet_ui_update(&world);
    rt_kprintf("[pet] fixed-room 390x450; eight directions; status/travel; session-only\n");
    return RT_EOK;
}
void pet_app_poll(void)
{
    uint32_t ms = sync_clock();
    display_elapsed += ms;
    if (display_elapsed >= 40)
    {
        display_elapsed %= 40;
        pet_ui_update(&world);
    }
}
