#include "pet_app.h"
#include "pet_model.h"
#include "pet_selftest.h"
#include "pet_ui.h"

static pet_model_t model;
static rt_tick_t previous_tick;
static uint32_t tick_fraction;
static uint32_t message_ms;
static uint32_t display_elapsed;
static const char *message;

static uint32_t sync_clock(void)
{
    rt_tick_t now = rt_tick_get();
    uint32_t elapsed = pet_clock_elapsed(previous_tick, now, RT_TICK_PER_SECOND, &tick_fraction);
    previous_tick = now;
    pet_model_advance(&model, elapsed);
    message_ms = elapsed >= message_ms ? 0 : message_ms - elapsed;
    if (!message_ms)
        message = NULL;
    return elapsed;
}

static void action(pet_event_t event)
{
    static const char *errors[] = {NULL, "One moment, please.", "A short break between treats.",
                                  "Already full. Try another activity.", "Sleeping. Tap Wake first.", "Unknown action."};
    sync_clock();
    pet_result_t result = pet_model_event(&model, event);
    message = errors[result];
    message_ms = message ? 2500 : 0;
    pet_ui_update(&model, message);
    rt_kprintf("[pet] event=%u result=%u food=%u mood=%u rest=%u\n",
               (unsigned)event, (unsigned)result, (unsigned)model.food,
               (unsigned)model.mood, (unsigned)model.sleeping);
}

static void report_test(const char *name, bool passed)
{
    rt_kprintf("[pet:SELFTEST] %s %s\n", name, passed ? "PASS" : "FAIL");
}

rt_err_t pet_app_start(void)
{
    if (!pet_selftest_run(report_test))
    {
        rt_kprintf("[pet:SELFTEST] all FAIL; UI not started\n");
        return -RT_ERROR;
    }
    rt_kprintf("[pet:SELFTEST] all PASS\n");
    pet_model_init(&model);
    previous_tick = rt_tick_get();
    tick_fraction = 0;
    message_ms = 0;
    display_elapsed = 0;
    message = NULL;
    pet_ui_create(action);
    pet_ui_update(&model, NULL);
    rt_kprintf("[pet] ready food=70 mood=60; session-only, no persistent storage\n");
    return RT_EOK;
}

void pet_app_poll(void)
{
    uint32_t elapsed = sync_clock();
    /* Refresh text at a modest rate; callbacks update immediately. */
    if (elapsed >= 100U - display_elapsed)
    {
        display_elapsed = 0;
        pet_ui_update(&model, message);
    }
    else
        display_elapsed += elapsed;
}
