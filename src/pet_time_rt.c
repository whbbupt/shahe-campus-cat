/* SDK v2.5.1 rt_device/rtc example and drv_rtc.c: uint32_t GET/SET_TIME. */
#include "pet_time_rt.h"
#include <rtthread.h>
#include <rtdevice.h>
static pt_clock_t clock_value;
static uint32_t poll_elapsed;
static bool failed;
static bool read_rtc(uint32_t *utc)
{
    rt_device_t device=rt_device_find("rtc");pt_date_t date;
    return device && rt_device_control(device,RT_DEVICE_CTRL_RTC_GET_TIME,utc)==RT_EOK && pt_decode(*utc,&date);
}
void pet_time_start(bool trusted)
{
    clock_value=(pt_clock_t){0};clock_value.trusted=trusted;poll_elapsed=0;failed=false;
    uint32_t utc;
    if(trusted && read_rtc(&utc)){clock_value.utc=utc;clock_value.valid=true;clock_value.state=PT_RTC;}
    rt_kprintf("[pet:TIME] boot trusted=%u valid=%u utc=%u\n",(unsigned)trusted,(unsigned)clock_value.valid,(unsigned)clock_value.utc);
}
void pet_time_poll(uint32_t elapsed_ms)
{
    if(elapsed_ms<1000U-poll_elapsed){poll_elapsed+=elapsed_ms;return;}
    poll_elapsed=(uint32_t)(((uint64_t)poll_elapsed+elapsed_ms)%1000U);
    if(!clock_value.trusted || failed) return;
    uint32_t utc;
    clock_value.valid=read_rtc(&utc);
    if(clock_value.valid)clock_value.utc=utc;
    else clock_value.state=PT_ERROR;
    if(clock_value.valid && clock_value.state==PT_ERROR)clock_value.state=PT_RTC;
}
pt_result_t pet_time_sync(uint32_t utc)
{
    pt_date_t date;uint32_t actual=0;
    if(!pt_decode(utc,&date))return PT_IO_ERROR;
    rt_device_t device=rt_device_find("rtc");
    pt_result_t result=!device || rt_device_control(device,RT_DEVICE_CTRL_RTC_SET_TIME,&utc)!=RT_EOK ? PT_IO_ERROR
        : !read_rtc(&actual) || actual<utc || actual-utc>1 ? PT_VERIFY_ERROR : PT_OK;
    clock_value.result=result;poll_elapsed=0;
    failed=result!=PT_OK;clock_value.valid=!failed;
    if(!failed){clock_value.utc=actual;clock_value.trusted=true;clock_value.state=PT_SYNCED;}
    else clock_value.state=PT_ERROR;
    rt_kprintf("[pet:TIME] sync result=%u utc=%u readback=%u\n",(unsigned)result,(unsigned)utc,(unsigned)actual);
    return result;
}
pt_clock_t pet_time_snapshot(void) { return clock_value; }
