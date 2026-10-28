#ifndef HOST_RTTHREAD_H
#define HOST_RTTHREAD_H
#include <stdint.h>
typedef uint32_t rt_tick_t;
typedef int rt_err_t;
#define RT_EOK 0
#define RT_ERROR 1
#define RT_TICK_PER_SECOND 1000
rt_tick_t rt_tick_get(void);
int rt_kprintf(const char *, ...);
#endif
