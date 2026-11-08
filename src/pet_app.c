#include "pet_app.h"
#include "pet_model.h"
#include "pet_ui.h"
#include "pet_world.h"
#include "pet_save.h"
#include "pet_save_flash.h"
#include "pet_audio.h"
#include "pet_audio_rt.h"
#include "pet_time_rt.h"
#include <string.h>
static pet_world_t world;
static pet_save_t save;
static rt_tick_t previous_tick;
static uint32_t fraction, display_elapsed;
static unsigned save_status;
void pet_app_get_phone_status(pet_phone_status_t *status)
{
    if (!status) return;
    *status=(pet_phone_status_t){world.food,world.mood,(uint8_t)world.scene,
        (uint8_t)world.phase,world.volume,world.sound_mode};
}
static void show_save(unsigned status)
{
    save_status = status;
    pet_ui_set_save_status(status);
}
static void refresh_save_status(void)
{
    if (save.enabled && save_status != PET_SAVE_FAILED)
        show_save(ps_visible_saved(&save, &world) ? PET_SAVE_SAVED : PET_SAVE_PENDING);
}
static uint32_t sync_clock(void)
{
    rt_tick_t now = rt_tick_get();
    uint32_t ms = pet_clock_elapsed(previous_tick, now, RT_TICK_PER_SECOND, &fraction);
    previous_tick = now;
    pw_advance(&world, ms);
    pet_time_poll(ms);
    world.clock=pet_time_snapshot();
    pet_audio_sync(&world);
    return ms;
}
pt_result_t pet_app_sync_time_from(uint32_t utc,bool automatic)
{
    sync_clock();
    pt_result_t result=pet_time_sync_from(utc,automatic);
    world.clock=pet_time_snapshot();
    refresh_save_status();
    pet_ui_update(&world);
    return result;
}
pt_result_t pet_app_sync_time(uint32_t utc) { return pet_app_sync_time_from(utc,false); }
pt_clock_t pet_app_get_clock(void) { return world.clock; }
static pw_result_t command(pw_command_t cmd)
{
    sync_clock();
    pw_result_t result = pw_request(&world, cmd);
    pet_audio_sync(&world);
    refresh_save_status();
    pet_ui_update(&world);
    rt_kprintf("[pet] cmd=%u result=%u scene=%u phase=%u food=%u mood=%u\n", (unsigned)cmd, (unsigned)result,
               (unsigned)world.scene, (unsigned)world.phase, (unsigned)world.food, (unsigned)world.mood);
    return result;
}
static pw_result_t walk_to(float x, float y)
{
    sync_clock();
    pw_result_t result = pw_walk_to(&world, x, y);
    refresh_save_status();
    pet_ui_update(&world);
    rt_kprintf("[pet:WALK] x=%d y=%d result=%u\n", (int)x, (int)y, (unsigned)result);
    return result;
}
static void report(const char *name, bool ok)
{
    rt_kprintf("[pet:WORLD] %s %s\n", name, ok ? "PASS" : "FAIL");
}
static void audio_report(const char *name, bool ok)
{
    rt_kprintf("[pet:AUDIO-TEST] %s %s\n", name, ok ? "PASS" : "FAIL");
}
static void save_report(const char *name, bool ok)
{
    rt_kprintf("[pet:SAVE-TEST] %s %s\n", name, ok ? "PASS" : "FAIL");
}
rt_err_t pet_app_start(void)
{
    if (!pt_selftest(report) || !pw_selftest(report) || !ps_selftest(save_report) || !pa_selftest(audio_report))
    {
        rt_kprintf("[pet] selftest FAIL; UI not started\n");
        return -RT_ERROR;
    }
    rt_kprintf("[pet:WORLD] all PASS\n[pet:SAVE-TEST] all PASS (RAM only)\n");
    pw_init(&world);
    memset(&save, 0, sizeof(save));
    ps_io_t io;
    ps_result_t loaded = PS_IO_ERROR;
    if (ps_flash_io(&io)) loaded = ps_open(&save, io, &world);
    pet_time_start(world.clock.trusted);
    world.clock=pet_time_snapshot();
    pet_audio_start(&world);
    previous_tick = rt_tick_get();
    fraction = display_elapsed = 0;
    pet_ui_create(command);
    pet_ui_set_point_handler(walk_to);
    show_save(loaded == PS_OK ? PET_SAVE_SAVED : (loaded == PS_EMPTY ? PET_SAVE_PENDING : PET_SAVE_UNAVAILABLE));
    pet_ui_set_audio_available(pet_audio_available());
    pet_ui_update(&world);
    rt_kprintf("[pet:SAVE] load=%u seq=%u scene=%u food=%u mood=%u sleeping=%u\n", (unsigned)loaded,
               (unsigned)save.sequence, (unsigned)world.scene, (unsigned)world.food, (unsigned)world.mood,
               (unsigned)(world.phase == PW_SLEEP));
    return RT_EOK;
}
void pet_app_poll(void)
{
    uint32_t ms = sync_clock();
    if (save.enabled)
    {
        ps_result_t result = ps_poll(&save, &world);
        if (result == PS_OK || result == PS_UNCHANGED)
        {
            show_save(PET_SAVE_SAVED);
            if (result == PS_OK)
                rt_kprintf("[pet:SAVE] saved seq=%u scene=%u food=%u mood=%u sleeping=%u\n", (unsigned)save.sequence,
                           (unsigned)world.scene, (unsigned)world.food, (unsigned)world.mood,
                           (unsigned)(world.phase == PW_SLEEP));
        }
        else if (result == PS_IO_ERROR || result == PS_VERSION)
        {
            if (save_status != PET_SAVE_FAILED)
                rt_kprintf("[pet:SAVE] save failed result=%u; last committed snapshot retained\n", (unsigned)result);
            show_save(PET_SAVE_FAILED);
        }
        refresh_save_status();
    }
    display_elapsed += ms;
    if (display_elapsed >= 40)
    {
        display_elapsed %= 40;
        pet_ui_set_audio_available(pet_audio_available());
        pet_ui_update(&world);
    }
}
