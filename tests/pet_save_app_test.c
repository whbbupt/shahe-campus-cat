#include "pet_app.h"
#include "pet_save_flash.h"
#include "pet_ui.h"
#include "pet_audio.h"
#include <rtdevice.h>
#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
static uint32_t rtc_value;
rt_device_t rt_device_find(const char *name) { assert(!strcmp(name,"rtc")); return (void *)1; }
int rt_device_control(rt_device_t d,int command,void *v) { (void)d; if(command==RT_DEVICE_CTRL_RTC_SET_TIME)rtc_value=*(uint32_t *)v; else *(uint32_t *)v=rtc_value; return RT_EOK; }
static uint8_t data[PS_AREA_SIZE];
static rt_tick_t tick;
static pet_world_t snapshot;
static pet_ui_action_fn action;
static unsigned status;
static int writes, calls, capacity=16*1024*1024, alignment=4096;
static bool fail_write;
static pa_observer_t sound_observer;
static unsigned sound_events;
void pet_audio_start(const pet_world_t *w) { pa_observer_init(&sound_observer, w); }
void pet_audio_sync(const pet_world_t *w) { if (pa_observe(&sound_observer, w)) sound_events++; }
bool pet_audio_available(void) { return true; }
void pet_ui_set_audio_available(bool ready) { assert(ready); }
rt_tick_t rt_tick_get(void) { return tick; }
int rt_kprintf(const char *f, ...) { (void)f; return 0; }
void pet_ui_set_point_handler(pet_ui_point_fn handler)
{
    (void)handler;
}
void pet_ui_create(pet_ui_action_fn f) { action=f; }
void pet_ui_update(const pet_world_t *w) { snapshot=*w; }
void pet_ui_set_save_status(unsigned s) { status=s; }
int rt_flash_get_total_size(uint32_t a) { assert(a==0x12DA8000); return capacity; }
int rt_flash_get_erase_alignment(uint32_t a) { (void)a; return alignment; }
static unsigned offset(uint32_t a, int n)
{
    assert(a>=0x12DA8000 && n>=0 && a+(unsigned)n<=0x12DAA000); return a-0x12DA8000;
}
int rt_flash_read(uint32_t a,uint8_t *p,int n) { calls++; memcpy(p,data+offset(a,n),(size_t)n); return n; }
int rt_flash_write(uint32_t a,const uint8_t *p,int n)
{
    calls++; writes++; unsigned o=offset(a,n); if(fail_write) return 0;
    for(int i=0;i<n;i++) { assert((data[o+i]&p[i])==p[i]); data[o+i]&=p[i]; } return n;
}
int rt_flash_erase(uint32_t a,int n) { calls++; assert(a%4096==0 && n==4096); memset(data+offset(a,n),255,(size_t)n); return RT_EOK; }
static void step(unsigned ms)
{
    for(unsigned i=0;i<ms;i+=20) { tick+=20; pet_app_poll(); }
}
static void reboot(void) { tick=0; assert(pet_app_start()==RT_EOK); step(40); }
int main(void)
{
    memset(data,255,sizeof(data));
    assert(pet_app_start()==RT_EOK && writes==0 && status==PET_SAVE_PENDING);
    assert(action(PW_SOUND_NEXT) == PW_OK && snapshot.sound_mode == 1);
    assert(action(PW_SOUND_NEXT) == PW_OK && snapshot.sound_mode == 0);
    step(14000); reboot();
    assert(snapshot.sound_mode == 0 && sound_events == 0);
    assert(action(PW_SOUND_NEXT) == PW_OK && snapshot.sound_mode == 2);
    step(14000);
    memset(data,255,sizeof(data)); writes = 0; reboot();
    /* Startup SAVE selftests must not touch the real flash writer. */
    step(3300); assert(writes==2 && status==PET_SAVE_SAVED);
    int before_save = writes;
    assert(action(PW_TOUCH)==PW_OK && status==PET_SAVE_PENDING);
    assert(writes==before_save); step(1000); assert(status==PET_SAVE_PENDING);
    reboot(); assert(snapshot.mood==60 && status==PET_SAVE_SAVED);

    assert(action(PW_TOUCH)==PW_OK); assert(status==PET_SAVE_PENDING);
    step(1000); assert(status==PET_SAVE_PENDING);
    step(500); assert(snapshot.mood==68 && status==PET_SAVE_PENDING);
    step(10500); assert(status==PET_SAVE_SAVED); reboot(); assert(snapshot.mood==68);
    assert(action(PW_GO_CANTEEN)==PW_OK && status==PET_SAVE_PENDING); step(15000);
    reboot(); assert(snapshot.scene==PW_CANTEEN && snapshot.mood==68);
    assert(action(PW_EAT)==PW_OK); step(18000); assert(snapshot.food==90);
    reboot(); assert(snapshot.food==90 && snapshot.action==PW_NONE);
    assert(action(PW_GO_DORM)==PW_OK); step(13000); assert(action(PW_REST)==PW_OK);
    step(20000); reboot(); assert(snapshot.phase==PW_SLEEP);
    assert(action(PW_REST)==PW_OK && status==PET_SAVE_PENDING); step(12000); reboot(); assert(snapshot.phase!=PW_SLEEP);
    fail_write=true; assert(action(PW_TOUCH)==PW_OK); step(15000); assert(status==PET_SAVE_FAILED);
    assert(action(PW_NONE)==PW_INVALID && status==PET_SAVE_FAILED);
    fail_write=false; step(11000); assert(status==PET_SAVE_SAVED); reboot(); assert(snapshot.mood==76);
    for (int i=0;i<3;i++) { assert(action(PW_TOUCH)==PW_OK && status==PET_SAVE_PENDING); step(14000); }
    reboot(); assert(snapshot.mood==100 && status==PET_SAVE_SAVED);
    int unchanged_writes=writes;
    assert(action(PW_TOUCH)==PW_FULL && status==PET_SAVE_SAVED && writes==unchanged_writes);
    puts("[host:save-app] immediate pending, mid-action reboot, travel/wake and rejected-no-change feedback PASS");

    puts("[host:save-app] real app/driver callbacks: autosave, scenes, meals, sleep/wake, reboot, failure/retry PASS");
    assert(action(PW_ORDER) == PW_OK && status == PET_SAVE_PENDING);
    step(4000);
    assert(status == PET_SAVE_SAVED);
    reboot();
    assert(snapshot.order == PW_ORDER_DELIVERING && snapshot.order_due > snapshot.now);
    step((uint32_t)(snapshot.order_due - snapshot.now) + 40);
    assert(snapshot.order == PW_ORDER_READY && status == PET_SAVE_PENDING);
    step(11000);
    reboot();
    assert(snapshot.order == PW_ORDER_READY && status == PET_SAVE_SAVED);
    assert(action(PW_TAKEOUT) == PW_OK);
    step(22000);
    assert(status == PET_SAVE_SAVED && snapshot.order == PW_ORDER_NONE && snapshot.food == 100);
    reboot();
    assert(snapshot.order == PW_ORDER_NONE && snapshot.food == 100);
    puts("[host:save-app] actual app order countdown, ready and pickup across reboot PASS");
    assert(action(PW_GO_CLASSROOM) == PW_OK);
    step(15000);
    assert(action(PW_STUDY) == PW_OK);
    step(16000);
    assert(action(PW_GO_CANTEEN) == PW_OK);
    step(15000);
    assert(action(PW_EAT) == PW_OK);
    step(14000);
    assert(action(PW_GO_DORM) == PW_OK);
    step(15000);
    assert(action(PW_REST) == PW_OK);
    step(15000);
    assert(action(PW_REST) == PW_OK);
    step(600);
    assert(snapshot.journal_progress == 7);
    assert(action(PW_JOURNAL_CLAIM) == PW_OK && status == PET_SAVE_PENDING);
    step(14000);
    reboot();
    assert(snapshot.journal_page == 2 && snapshot.journal_stamps == 1 && status == PET_SAVE_SAVED);
    assert(action(PW_GO_CLASSROOM) == PW_OK);
    step(15000);
    assert(action(PW_MEMORY_START) == PW_OK);
    step(6000);
    assert(snapshot.phase == PW_MINIGAME);
    reboot();
    assert(snapshot.phase == PW_IDLE && snapshot.memory_wins == 0 && snapshot.journal_page == 2);
    puts("[host:save-app] journal claim and unfinished memory game across actual app reboot PASS");
    ps_io_t io; capacity=8*1024*1024; assert(!ps_flash_io(&io)); reboot(); assert(status==PET_SAVE_UNAVAILABLE);
    assert(action(PW_NONE)==PW_INVALID && status==PET_SAVE_UNAVAILABLE);
    capacity=16*1024*1024; alignment=8192; assert(!ps_flash_io(&io)); alignment=4096; assert(ps_flash_io(&io));
    uint8_t b[8]; int before=calls;
    assert(!io.read(NULL,8190,b,8) && !io.write(NULL,8190,b,8) && !io.erase(NULL,4095,4096));
    assert(calls==before);
    puts("[host:save-app] capacity, erase-granularity, region bounds and truthful fallback PASS; no physical verdict");
    pet_phone_status_t phone;
    pet_app_get_phone_status(&phone);
    assert(phone.food==snapshot.food && phone.mood==snapshot.mood && phone.scene==snapshot.scene &&
        phone.phase==snapshot.phase && phone.volume==snapshot.volume && phone.sound_mode==snapshot.sound_mode);
    puts("[host:save-app] main-thread phone snapshot matches actual application state PASS");
    memset(data,255,sizeof(data));reboot();
    uint32_t utc;assert(pt_parse((const uint8_t *)"20261108235959",14,&utc));
    assert(!pet_app_get_clock().valid);
    assert(pet_app_sync_time(utc)==PT_OK && pet_app_get_clock().valid);
    uint64_t elapsed=snapshot.now;
    assert(pet_app_sync_time(utc-3600)==PT_OK && snapshot.now==elapsed);
    step(14000);assert(status==PET_SAVE_SAVED);
    unsigned saved_writes=writes;rtc_value=utc+1;step(14000);
    assert(writes==saved_writes && status==PET_SAVE_SAVED);
    reboot();assert(pet_app_get_clock().valid && pet_app_get_clock().utc==utc+1);
    rtc_value=0;reboot();assert(!pet_app_get_clock().valid);
    puts("[host:save-app] phone sync, no time jumps, clock not dirty, reboot RTC trust and lost clock PASS");
    return 0;
}
