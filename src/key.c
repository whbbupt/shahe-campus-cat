/* T1 first version: raw edge polling, intentionally without debounce. */
#include "key.h"
#include "drivers/pin.h"

#if !defined(BSP_KEY1_PIN) || !defined(BSP_KEY1_ACTIVE_HIGH)
#error "T1 requires the board KEY1 pin and active-high configuration"
#endif
#if BSP_KEY1_ACTIVE_HIGH != 1
#error "T1 expects the verified active-high KEY1 board configuration"
#endif

struct key_state
{
    int previous_level;
    rt_uint32_t press_count;
    rt_uint32_t color_index;
};

static rt_mq_t key_queue;

static void key_state_init(struct key_state *state, int initial_level)
{
    state->previous_level = initial_level;
    state->press_count = 0;
    state->color_index = 0;
}

/* Returns 1 for a rising edge, 0 for no event, or a negative error. */
static int key_sample(struct key_state *state, int level,
                      struct t1_key_event *event)
{
    if (level != PIN_LOW && level != PIN_HIGH)
        return -RT_EINVAL;

    rt_bool_t pressed = state->previous_level == PIN_LOW && level == PIN_HIGH;
    state->previous_level = level;
    if (!pressed)
        return 0;

    state->press_count++;
    state->color_index = (state->color_index + 1U) % T1_RGB_COLOR_COUNT;
    event->press_count = state->press_count;
    event->color_index = state->color_index;
    return 1;
}

static rt_bool_t self_test_fail(const char *name)
{
    rt_kprintf("[t1] SELFTEST %s FAIL\n", name);
    return RT_FALSE;
}

rt_bool_t t1_key_self_test(void)
{
    const int samples[] = {1, 1, 0, 1, 0, 1, 1, 0, 1};
    const rt_uint32_t colors[] = {1, 2, 0, 1};
    struct key_state state;
    struct t1_key_event event;
    unsigned int events = 0;

    key_state_init(&state, PIN_LOW);
    if (key_sample(&state, PIN_LOW, &event) != 0)
        return self_test_fail("idle");
    for (unsigned int i = 0; i < sizeof(samples) / sizeof(samples[0]); i++)
    {
        int result = key_sample(&state, samples[i], &event);
        if (result < 0)
            return self_test_fail("sequence");
        if (result == 1)
        {
            if (events >= sizeof(colors) / sizeof(colors[0]) ||
                event.color_index != colors[events] || event.press_count != events + 1U)
                return self_test_fail("sequence");
            events++;
        }
    }
    if (events != sizeof(colors) / sizeof(colors[0]))
        return self_test_fail("hold-release");
    rt_kprintf("[t1] SELFTEST sequence/hold/release PASS\n");

    /* A key held during boot must be released before the first real press. */
    key_state_init(&state, PIN_HIGH);
    if (key_sample(&state, PIN_HIGH, &event) != 0 ||
        key_sample(&state, PIN_LOW, &event) != 0 ||
        key_sample(&state, PIN_HIGH, &event) != 1 ||
        event.press_count != 1 || event.color_index != 1)
        return self_test_fail("boot-held");
    rt_kprintf("[t1] SELFTEST boot-held PASS\n");

    /* Demonstrates the known raw-edge risk, not a physical bounce result. */
    key_state_init(&state, PIN_LOW);
    if (key_sample(&state, PIN_HIGH, &event) != 1 ||
        key_sample(&state, PIN_LOW, &event) != 0 ||
        key_sample(&state, PIN_HIGH, &event) != 1 || event.press_count != 2)
        return self_test_fail("raw-bounce");
    rt_kprintf("[t1] SELFTEST raw-bounce PASS (2 edges; no debounce)\n");

    rt_uint32_t count_before = state.press_count;
    if (key_sample(&state, -1, &event) != -RT_EINVAL ||
        state.press_count != count_before)
        return self_test_fail("invalid-level");
    rt_kprintf("[t1] SELFTEST all PASS (logic only)\n");
    return RT_TRUE;
}

static void key_worker(void *parameter)
{
    struct key_state state;
    struct t1_key_event event;
    int initial_level = rt_pin_read(BSP_KEY1_PIN);
    (void)parameter;

    if (initial_level != PIN_LOW && initial_level != PIN_HIGH)
    {
        rt_kprintf("[t1] ERROR: invalid initial key level %d; input stopped\n", initial_level);
        return;
    }
    key_state_init(&state, initial_level);
    while (1)
    {
        int result = key_sample(&state, rt_pin_read(BSP_KEY1_PIN), &event);
        if (result < 0)
        {
            rt_kprintf("[t1] ERROR: invalid key sample (%d); input stopped\n", result);
            return;
        }
        if (result == 1)
        {
            rt_err_t status = rt_mq_send(key_queue, &event, sizeof(event));
            if (status != RT_EOK)
            {
                rt_kprintf("[t1] ERROR: key queue send failed (%d); input stopped\n", status);
                return;
            }
        }
        rt_thread_mdelay(T1_KEY_POLL_MS);
    }
}

rt_err_t t1_key_start(void)
{
    if (key_queue != RT_NULL)
        return -RT_EBUSY;
    if (rt_device_find("pin") == RT_NULL)
        return -RT_ERROR;

    key_queue = rt_mq_create("t1keyq", sizeof(struct t1_key_event),
                             T1_KEY_QUEUE_DEPTH, RT_IPC_FLAG_FIFO);
    if (key_queue == RT_NULL)
        return -RT_ENOMEM;

    /* KEY1 is also a power/download key. Keep the BSP pin mode and pulls. */
    rt_thread_t thread = rt_thread_create("t1key", key_worker, RT_NULL, 1024, 9, 10);
    if (thread == RT_NULL)
    {
        rt_mq_delete(key_queue);
        key_queue = RT_NULL;
        return -RT_ENOMEM;
    }
    rt_err_t result = rt_thread_startup(thread);
    if (result != RT_EOK)
    {
        rt_thread_delete(thread);
        rt_mq_delete(key_queue);
        key_queue = RT_NULL;
    }
    return result;
}

rt_err_t t1_key_receive(struct t1_key_event *event)
{
    if (key_queue == RT_NULL || event == RT_NULL)
        return -RT_EINVAL;
    return rt_mq_recv(key_queue, event, sizeof(*event), 0);
}
