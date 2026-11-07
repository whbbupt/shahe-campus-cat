#ifndef BLE_HOST_RTTHREAD_H
#define BLE_HOST_RTTHREAD_H
#include <stdint.h>
#include <stddef.h>
typedef uint32_t rt_tick_t;
typedef int rt_err_t;
typedef intptr_t rt_base_t;
#define RT_USING_FINSH 1
typedef void *rt_mq_t;
typedef void *rt_thread_t;
#define RT_EOK 0
#define RT_ERROR 1
#define RT_ENOMEM 12
#define RT_IPC_FLAG_FIFO 0
rt_mq_t rt_mq_create(const char *,size_t,size_t,int);
int rt_mq_send(rt_mq_t,const void *,size_t);
int rt_mq_recv(rt_mq_t,void *,size_t,int);
int rt_mq_delete(rt_mq_t);
rt_thread_t rt_thread_create(const char *,void (*)(void *),void *,unsigned,unsigned,unsigned);
int rt_thread_startup(rt_thread_t);
int rt_thread_delete(rt_thread_t);
rt_tick_t rt_tick_get(void);
rt_tick_t rt_tick_from_millisecond(int);
void *rt_malloc(size_t);
void rt_free(void *);
int rt_snprintf(char *,size_t,const char *,...);
int rt_kprintf(const char *,...);
#endif
