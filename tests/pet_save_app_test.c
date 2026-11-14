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
static uint32_t record_crc(const uint8_t *record)
{
    uint32_t crc = ~0U;
    for (unsigned i = 0; i < 56; i++)
    {
        crc ^= record[i];
        for (unsigned bit = 0; bit < 8; bit++)
            crc = (crc >> 1) ^ (0xEDB88320U & (0U - (crc & 1U)));
    }
    return ~crc;
}
static void starting_values(void)
{
    memset(data, 255, sizeof(data));
    reboot();
    assert(snapshot.wallet.coins == 100 && snapshot.food == 50 && snapshot.mood == 50);
    assert(status == PET_SAVE_PENDING);
    ps_io_t io;
    pet_save_t journal;
    pet_world_t old;
    pw_init(&old);
    old.wallet = (pe_wallet_t){21, 3, 1};
    old.dishes = 0x015A;
    old.order = PW_ORDER_READY;
    old.order_choice = 2;
    assert(ps_flash_io(&io) && ps_open(&journal, io, &old) == PS_EMPTY);
    assert(ps_save(&journal, &old) == PS_OK);
    data[4] = 12;
    uint32_t crc = record_crc(data);
    for (unsigned i = 0; i < 4; i++) data[56 + i] = (uint8_t)(crc >> (8 * i));
    fail_write = true;
    reboot();
    assert(status == PET_SAVE_FAILED && data[4] == 12 && data[46] == 21);
    fail_write = false;
    reboot();
    assert(status == PET_SAVE_SAVED && snapshot.wallet.coins == 100 && snapshot.food == 50 && snapshot.mood == 50);
    assert(snapshot.wallet.owned == 3 && snapshot.wallet.equipped == 1 && snapshot.dishes == 0x015A);
    assert(snapshot.order == PW_ORDER_READY && snapshot.order_choice == 2);
    assert(ps_open(&journal, io, &old) == PS_OK && journal.saved[4] == PS_SCHEMA);
    old.wallet.coins = 88; old.food = 42; old.mood = 53;
    assert(ps_save(&journal, &old) == PS_OK);
    reboot();
    assert(status == PET_SAVE_SAVED && snapshot.wallet.coins == 88 && snapshot.food == 42 && snapshot.mood == 53);
    puts("[host:save-app] new values, migration failure/retry, preserved progress and subsequent reboot PASS");
}
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
    reboot(); assert(snapshot.mood==50 && status==PET_SAVE_SAVED);

    assert(action(PW_TOUCH)==PW_OK); assert(status==PET_SAVE_PENDING);
    step(1000); assert(status==PET_SAVE_PENDING);
    step(500); assert(snapshot.mood==53 && status==PET_SAVE_PENDING);
    step(10500); assert(status==PET_SAVE_SAVED); reboot(); assert(snapshot.mood==53);
    assert(action(PW_GO_CANTEEN)==PW_OK && status==PET_SAVE_PENDING); step(15000);
    reboot(); assert(snapshot.scene==PW_CANTEEN && snapshot.mood==53);
    assert(action(PW_EAT)==PW_OK); step(18000); assert(snapshot.food==70);
    reboot(); assert(snapshot.food==70 && snapshot.action==PW_NONE);
    assert(action(PW_GO_DORM)==PW_OK); step(13000); assert(action(PW_REST)==PW_OK);
    step(20000); reboot(); assert(snapshot.phase==PW_SLEEP);
    assert(action(PW_REST)==PW_OK && status==PET_SAVE_PENDING); step(12000); reboot(); assert(snapshot.phase!=PW_SLEEP);
    fail_write=true; assert(action(PW_TOUCH)==PW_OK); step(15000); assert(status==PET_SAVE_FAILED);
    assert(action(PW_NONE)==PW_INVALID && status==PET_SAVE_FAILED);
    fail_write=false; step(11000); assert(status==PET_SAVE_SAVED); reboot(); assert(snapshot.mood==56);
    for (int i=0;i<15;i++) { assert(action(PW_TOUCH)==PW_OK && status==PET_SAVE_PENDING); step(14000); }
    reboot(); assert(snapshot.mood==100 && status==PET_SAVE_SAVED);
    /* Reboot restores the last saved cooldown conservatively. */
    step(PW_TOUCH_COOLDOWN_MS);
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
    unsigned food_after_pickup = snapshot.food + pe_order(snapshot.order_choice)->food;
    assert(action(PW_TAKEOUT) == PW_OK);
    step(22000);
    assert(status == PET_SAVE_SAVED && snapshot.order == PW_ORDER_NONE && snapshot.food == food_after_pickup);
    reboot();
    assert(snapshot.order == PW_ORDER_NONE && snapshot.food == food_after_pickup);
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
    unsigned coins = snapshot.wallet.coins;
    assert(action(PW_JOURNAL_CLAIM) == PW_INVALID && snapshot.wallet.coins == coins);
    step(14000); reboot();
    assert(snapshot.wallet.coins == coins && status == PET_SAVE_SAVED);
    assert(action(PW_GO_CLASSROOM) == PW_OK);
    step(15000);
    assert(action(PW_MEMORY_START) == PW_OK);
    step(6000);
    assert(snapshot.phase == PW_MINIGAME);
    reboot();
    assert(snapshot.phase == PW_IDLE && snapshot.memory_wins == 0 && snapshot.wallet.coins == coins);
    puts("[host:save-app] wallet and unfinished memory game across actual app reboot PASS");
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
    assert(pet_app_sync_time_from(utc,true)==PT_OK&&pet_app_get_clock().automatic);
    step(1000);assert(pet_app_get_clock().automatic);
    rtc_value=0;reboot();assert(!pet_app_get_clock().valid);
    puts("[host:save-app] phone sync, no time jumps, clock not dirty, reboot RTC trust and lost clock PASS");
    memset(data,255,sizeof(data)); reboot();
    assert(snapshot.wallet.coins == 100 && !snapshot.wallet.owned);
    ps_io_t economy_io;
    pet_save_t economy_save;
    pet_world_t economy_start = snapshot;
    assert(ps_flash_io(&economy_io) && ps_open(&economy_save, economy_io, &economy_start) == PS_EMPTY);
    economy_start.wallet.coins = 30;
    assert(ps_save(&economy_save, &economy_start) == PS_OK);
    reboot();
    assert(action(PW_GO_CLASSROOM) == PW_OK); step(15000);
    assert(action(PW_MEMORY_START) == PW_OK); step(6000);
    for (unsigned round=0; round<3; round++)
    {
        if (snapshot.memory.phase == PM_SHOW)
        {
            step(pm_remaining(&snapshot.memory));
            /* The view snapshot can lag the model by one 40 ms refresh. */
            if (snapshot.memory.phase == PM_SHOW) step(40);
        }
        assert(snapshot.memory.phase == PM_INPUT);
        unsigned count=snapshot.memory.length;
        for(unsigned i=0;i<count;i++) assert(action((pw_command_t)(PW_MEMORY_0+snapshot.memory.sequence[i]))==PW_OK);
    }
    assert(snapshot.wallet.coins==40 && snapshot.memory_wins==1);
    step(1600); assert(action(PW_GO_DORM)==PW_OK); step(15000);
    assert(action(PW_BUY_PLANT)==PW_OK && snapshot.wallet.coins==0 && status==PET_SAVE_PENDING);
    assert(snapshot.wallet.owned==1 && snapshot.wallet.equipped==1);
    step(14000); assert(status==PET_SAVE_SAVED); reboot();
    assert(snapshot.wallet.coins==0 && snapshot.wallet.owned==1 && snapshot.wallet.equipped==1);
    assert(action(PW_BUY_PLANT)==PW_OK && !snapshot.wallet.equipped && status==PET_SAVE_PENDING);
    step(14000); reboot(); assert(!snapshot.wallet.coins && !snapshot.wallet.equipped && snapshot.wallet.owned==1);
    assert(action(PW_BUY_SNACK)==PW_NO_COINS && status==PET_SAVE_SAVED && !snapshot.wallet.coins);
    assert(action(PW_GO_CANTEEN)==PW_OK); step(15000); step(60000);
    assert(action(PW_MEAL_GAME)==PW_OK); step(10000);
    assert(snapshot.challenge.phase==PG_INPUT);
    assert(action((pw_command_t)(PW_MEMORY_0+(snapshot.challenge.targets[0]+1)%4))==PW_OK);
    assert(snapshot.wallet.coins==2 && snapshot.coin_gain==2);
    assert(action(PW_MEMORY_CANCEL)==PW_OK); step(14000); reboot();
    assert(snapshot.wallet.coins==2 && snapshot.wallet.owned==1 && !snapshot.wallet.equipped && snapshot.memory_wins==1);
    puts("[host:save-app] actual earn->buy furniture->zero balance->free store->save/reboot->participation income, no repeated starter grant PASS");
    pt_date_t day_date={2026,11,8,12,0,0};uint32_t day_utc;assert(pt_encode(&day_date,&day_utc));
    assert(pet_app_sync_time(day_utc)==PT_OK);
    unsigned before_daily=snapshot.wallet.coins;
    assert(action(PW_DAILY_CHECKIN)==PW_OK&&snapshot.wallet.coins==before_daily+6&&status==PET_SAVE_PENDING);
    unsigned claimed_day=snapshot.checkin_day;step(14000);assert(status==PET_SAVE_SAVED);reboot();
    assert(snapshot.checkin_day==claimed_day&&snapshot.wallet.coins==before_daily+6);
    assert(action(PW_DAILY_CHECKIN)==PW_COOLDOWN&&status==PET_SAVE_SAVED);
    assert(pet_app_sync_time(day_utc+86400)==PT_OK);
    assert(action(PW_DAILY_CHECKIN)==PW_OK&&snapshot.wallet.coins==before_daily+12&&status==PET_SAVE_PENDING);
    step(14000);reboot();assert(snapshot.checkin_day==claimed_day+1&&snapshot.wallet.coins==before_daily+12);
    puts("[host:save-app] actual RTC sync->daily coins/date->pending/saved->reboot same-day denied->next-day income PASS");


    memset(data,255,sizeof(data));rtc_value=0;reboot();
    pt_date_t night={2026,11,9,23,0,0};uint32_t night_utc;assert(pt_encode(&night,&night_utc));
    assert(pet_app_sync_time(night_utc)==PT_OK);
    assert(action(PW_EXIT)==PW_CURFEW && snapshot.scene==PW_DORM);
    assert(action(PW_REST)==PW_OK);step(5000);assert(snapshot.phase==PW_SLEEP);
    assert(action(PW_REST)==PW_OK);step(600);assert(action(PW_EXIT)==PW_CURFEW);
    assert(action(PW_DELIVERY_LAST)==PW_OK && snapshot.wallet.coins==95 && snapshot.order_choice==6);
    step(14000);assert(status==PET_SAVE_SAVED);reboot();
    assert(snapshot.order_choice==6 && snapshot.wallet.coins==95 && snapshot.order==PW_ORDER_DELIVERING);
    step(40000);assert(snapshot.order==PW_ORDER_READY && snapshot.order_choice==6);
    unsigned food_after_congee = snapshot.food + pe_order(6)->food;
    assert(action(PW_TAKEOUT)==PW_OK);step(22000);
    assert(status == PET_SAVE_SAVED && snapshot.order == PW_ORDER_NONE);
    reboot();
    assert(snapshot.order==PW_ORDER_NONE && !snapshot.order_choice && snapshot.food==food_after_congee && snapshot.wallet.coins==95);
    assert(pet_app_sync_time(night_utc-1)==PT_OK);assert(action(PW_EXIT)==PW_OK);
    rtc_value=night_utc;step(10000);assert(snapshot.scene==PW_DORM && snapshot.phase==PW_IDLE && snapshot.curfew_events);
    puts("[host:save-app] actual RTC-before-navigation guard, night wake/order/congee/save/reboot/pickup and crossing curfew PASS");

    starting_values();
    return 0;
}
