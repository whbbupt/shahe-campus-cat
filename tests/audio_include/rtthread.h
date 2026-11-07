#ifndef HOST_RTTHREAD_H
#define HOST_RTTHREAD_H
#include <stdint.h>
#include <stddef.h>
typedef uint32_t rt_tick_t;
typedef int rt_err_t;
typedef void *rt_thread_t;
struct rt_mutex { int value; };
#define RT_EOK 0
#define RT_ERROR 1
#define RT_WAITING_FOREVER -1
#define RT_IPC_FLAG_PRIO 1
rt_tick_t rt_tick_get(void);
rt_tick_t rt_tick_from_millisecond(int);
int rt_kprintf(const char *, ...);
int rt_mutex_init(struct rt_mutex *, const char *, int);
int rt_mutex_take(struct rt_mutex *, int);
int rt_mutex_release(struct rt_mutex *);
rt_thread_t rt_thread_create(const char *, void (*)(void *), void *, unsigned, unsigned, unsigned);
int rt_thread_startup(rt_thread_t);
int rt_thread_delete(rt_thread_t);
void rt_thread_mdelay(int);
#endif
