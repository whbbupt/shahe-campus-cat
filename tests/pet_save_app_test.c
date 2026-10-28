#include "pet_app.h"
#include "pet_save_flash.h"
#include "pet_ui.h"
#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
static uint8_t data[PS_AREA_SIZE];
static rt_tick_t tick;
static pet_world_t snapshot;
static pet_ui_action_fn action;
static unsigned status;
static int writes, calls, capacity=16*1024*1024, alignment=4096;
static bool fail_write;
rt_tick_t rt_tick_get(void) { return tick; }
int rt_kprintf(const char *f, ...) { (void)f; return 0; }
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
    /* Startup SAVE selftests must not touch the real flash writer. */
    step(3300); assert(writes==2 && status==PET_SAVE_SAVED);
    int before_save = writes;
    assert(action(PW_TOUCH)==PW_OK && status==PET_SAVE_PENDING);
    assert(writes==before_save); step(1000); assert(status==PET_SAVE_PENDING);
    reboot(); assert(snapshot.mood==60 && status==PET_SAVE_SAVED);

    assert(action(PW_TOUCH)==PW_OK); assert(status==PET_SAVE_PENDING);
    step(1000); assert(status==PET_SAVE_PENDING);
    step(500); assert(snapshot.mood==70 && status==PET_SAVE_PENDING);
    step(10500); assert(status==PET_SAVE_SAVED); reboot(); assert(snapshot.mood==70);
    assert(action(PW_GO_CANTEEN)==PW_OK && status==PET_SAVE_PENDING); step(15000);
    reboot(); assert(snapshot.scene==PW_CANTEEN && snapshot.mood==70);
    assert(action(PW_EAT)==PW_OK); step(18000); assert(snapshot.food==85);
    reboot(); assert(snapshot.food==85 && snapshot.action==PW_NONE);
    assert(action(PW_GO_DORM)==PW_OK); step(13000); assert(action(PW_REST)==PW_OK);
    step(20000); reboot(); assert(snapshot.phase==PW_SLEEP);
    assert(action(PW_REST)==PW_OK && status==PET_SAVE_PENDING); step(12000); reboot(); assert(snapshot.phase!=PW_SLEEP);
    fail_write=true; assert(action(PW_TOUCH)==PW_OK); step(15000); assert(status==PET_SAVE_FAILED);
    assert(action(PW_NONE)==PW_INVALID && status==PET_SAVE_FAILED);
    fail_write=false; step(11000); assert(status==PET_SAVE_SAVED); reboot(); assert(snapshot.mood==80);
    for (int i=0;i<2;i++) { assert(action(PW_TOUCH)==PW_OK && status==PET_SAVE_PENDING); step(14000); }
    reboot(); assert(snapshot.mood==100 && status==PET_SAVE_SAVED);
    int unchanged_writes=writes;
    assert(action(PW_TOUCH)==PW_FULL && status==PET_SAVE_SAVED && writes==unchanged_writes);
    puts("[host:save-app] immediate pending, mid-action reboot, travel/wake and rejected-no-change feedback PASS");

    puts("[host:save-app] real app/driver callbacks: autosave, scenes, meals, sleep/wake, reboot, failure/retry PASS");
    ps_io_t io; capacity=8*1024*1024; assert(!ps_flash_io(&io)); reboot(); assert(status==PET_SAVE_UNAVAILABLE);
    assert(action(PW_NONE)==PW_INVALID && status==PET_SAVE_UNAVAILABLE);
    capacity=16*1024*1024; alignment=8192; assert(!ps_flash_io(&io)); alignment=4096; assert(ps_flash_io(&io));
    uint8_t b[8]; int before=calls;
    assert(!io.read(NULL,8190,b,8) && !io.write(NULL,8190,b,8) && !io.erase(NULL,4095,4096));
    assert(calls==before);
    puts("[host:save-app] capacity, erase-granularity, region bounds and truthful fallback PASS; no physical verdict");
    return 0;
}
