#include "pet_time_rt.h"
#include <rtdevice.h>
#include <assert.h>
#include <stdarg.h>
#include <string.h>
#include <stdio.h>
static bool missing,read_fail,write_fail,wrong_readback;
static unsigned gets,sets;
static uint32_t raw;
rt_device_t rt_device_find(const char *name){assert(!strcmp(name,"rtc"));return missing?NULL:(void *)1;}
int rt_device_control(rt_device_t device,int command,void *value){
    assert(device==(void *)1);
    if(command==RT_DEVICE_CTRL_RTC_SET_TIME){sets++;if(write_fail)return -1;raw=*(uint32_t *)value;return 0;}
    assert(command==RT_DEVICE_CTRL_RTC_GET_TIME);gets++;if(read_fail)return -1;
    *(uint32_t *)value=raw+(wrong_readback?10:0);return 0;
}
int rt_kprintf(const char *format,...){(void)format;return 0;}
int main(void){
    uint32_t t;assert(pt_parse((const uint8_t *)"20261108235959",14,&t));raw=t;
    pet_time_start(false);assert(!pet_time_snapshot().valid&&gets==0);
    pet_time_poll(900000);assert(sets==0&&!pet_time_snapshot().valid);
    assert(pet_time_sync(t)==PT_OK&&sets==1&&pet_time_snapshot().trusted&&pet_time_snapshot().state==PT_SYNCED);
    raw=t+1;pet_time_poll(999);assert(pet_time_snapshot().utc==t);pet_time_poll(1);assert(pet_time_snapshot().utc==t+1);
    pet_time_start(true);assert(pet_time_snapshot().valid&&pet_time_snapshot().state==PT_RTC);
    unsigned before=sets;assert(pet_time_sync(0)==PT_IO_ERROR&&sets==before);
    missing=true;assert(pet_time_sync(t)==PT_IO_ERROR&&!pet_time_snapshot().valid);missing=false;
    write_fail=true;assert(pet_time_sync(t)==PT_IO_ERROR);write_fail=false;
    wrong_readback=true;assert(pet_time_sync(t)==PT_VERIFY_ERROR&&!pet_time_snapshot().valid);wrong_readback=false;
    pet_time_poll(1000);assert(!pet_time_snapshot().valid); /* failed SET remains explicit until retry */
    assert(pet_time_sync(t)==PT_OK);
    read_fail=true;pet_time_poll(1000);assert(!pet_time_snapshot().valid&&pet_time_snapshot().state==PT_ERROR);read_fail=false;
    pet_time_poll(1000);assert(pet_time_snapshot().valid);
    assert(pet_time_sync(t-3600)==PT_OK&&pet_time_snapshot().utc==t-3600);
    raw=0;pet_time_start(true);assert(!pet_time_snapshot().valid);
    puts("[host:rtc] SDK GET/SET adapter, trust, throttle, rollover, readback, errors and retry PASS");return 0;
}
