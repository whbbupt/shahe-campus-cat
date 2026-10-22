#ifndef T1_KEY_H
#define T1_KEY_H

#include "rtthread.h"

#define T1_RGB_COLOR_COUNT 3U
#define T1_KEY_POLL_MS     5
#define T1_KEY_QUEUE_DEPTH 16

struct t1_key_event
{
    rt_uint32_t press_count;
    rt_uint32_t color_index;
};

/* Fixed-input logic tests run on the board at startup, before reading KEY1. */
rt_bool_t t1_key_self_test(void);
/* Uses the existing BSP KEY1 input configuration; call once after RGB init. */
rt_err_t t1_key_start(void);
/* Main-thread consumer; returns -RT_ETIMEOUT when the queue is empty. */
rt_err_t t1_key_receive(struct t1_key_event *event);

#endif
