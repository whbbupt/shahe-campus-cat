#include "pet_time.h"
#include "pet_world.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static uint32_t when(const char *s){uint32_t utc;assert(pt_parse((const uint8_t *)s,strlen(s),&utc));return utc;}
static void set_time(pet_world_t *w,const char *s){w->clock=(pt_clock_t){when(s),true,true,PT_SYNCED,PT_OK};}
static void start(pet_world_t *w,pw_scene_t scene,pw_command_t action,const char *at)
{
    pw_init(w);w->scene=scene;w->node=6;pw_point_t p=pw_node(scene,6);w->x=p.x;w->y=p.y;
    w->food=w->mood=50;set_time(w,at);assert(pw_request(w,action)==PW_OK);
    for(unsigned i=0;i<100 && w->phase!=PW_ACTION;i++)pw_advance(w,40);
    assert(w->phase==PW_ACTION);
}
int main(void)
{
    assert(pt_selftest(NULL));pt_date_t d;uint32_t utc;
    assert(when("20240101000000")==1704038400U);
    assert(when("20371231235959")==2145887999U);
    assert(pt_decode(when("20371231235959"),&d)&&d.year==2037&&d.second==59);
    assert(!pt_decode(2145888000U,&d)&&!pt_decode(0xffffffffU,&d));
    const char *bad[]={"20230228000000","20380101000000","20260229000000","20260431000000","20260001000000","20261301000000","20261100000000","20261108240000","20261108206000","20261108200060","2026x108200000"};
    for(unsigned i=0;i<sizeof(bad)/sizeof(*bad);i++)assert(!pt_parse((const uint8_t *)bad[i],strlen(bad[i]),&utc));
    assert(!pt_parse((const uint8_t *)"20261108220000",13,&utc));
    assert(pt_decode(when("20241231235959")+1,&d)&&d.year==2025&&d.month==1&&d.day==1&&d.hour==0);
    assert(pt_decode(when("20280228235959")+1,&d)&&d.month==2&&d.day==29);
    const char *bounds[]={"20261108055900","20261108060000","20261108075900","20261108080000","20261108165900","20261108170000","20261108185900","20261108190000"};
    const pt_band_t bands[]={PT_NIGHT,PT_MORNING,PT_MORNING,PT_DAY,PT_DAY,PT_DUSK,PT_DUSK,PT_NIGHT};
    for(unsigned i=0;i<8;i++)assert(pt_band(when(bounds[i]))==bands[i]);
    for(unsigned minute=0;minute<1440;minute++){
        uint32_t t=when("20261108000000")+minute*60;
        assert(pt_class_time(t)==((minute>=480&&minute<720)||(minute>=840&&minute<1020)));
        assert(pt_meal_time(t)==((minute>=360&&minute<540)||(minute>=660&&minute<840)||(minute>=1020&&minute<1200)));
        assert(pt_bedtime(t)==(minute>=1380||minute<360?2:minute>=1320?1:0));
    }
    pet_world_t w;start(&w,PW_CLASSROOM,PW_STUDY,"20261108080000");assert(w.food==43&&w.mood==54&&w.activity_kind==0);
    start(&w,PW_CLASSROOM,PW_STUDY,"20261108120000");assert(w.food==46&&w.mood==53&&w.activity_kind==1);
    start(&w,PW_CLASSROOM,PW_STUDY,"20261108230000");assert(w.food==46&&w.mood==51&&w.action_tired);
    uint64_t end=w.action_end,now=w.now;uint32_t settlements=w.settlements[PW_STUDY];
    set_time(&w,"20261108080000");assert(w.action_end==end&&w.now==now&&w.settlements[PW_STUDY]==settlements);
    pw_advance(&w,12000);assert(w.mood==51&&w.settlements[PW_STUDY]==1);
    start(&w,PW_CANTEEN,PW_EAT,"20261108110000");assert(w.food==70&&w.mood==50);
    start(&w,PW_CANTEEN,PW_EAT,"20261108230000");assert(w.food==60&&w.mood==51&&w.activity_kind==2);
    start(&w,PW_GYM,PW_WORKOUT,"20261108230000");set_time(&w,"20261108080000");pw_advance(&w,15000);assert(w.mood==54&&w.settlements[PW_WORKOUT]==1);
    pw_init(&w);set_time(&w,"20261108220000");assert(strstr(pw_state_text(&w),"准备睡觉"));set_time(&w,"20261108230000");assert(strstr(pw_state_text(&w),"有点困"));
    w.clock.valid=false;assert(!strstr(pw_state_text(&w),"困"));
    assert(pw_request(&w,PW_REST)==PW_OK);
    for(unsigned i=0;i<500 && w.phase!=PW_SLEEP;i++)pw_advance(&w,40);
    assert(w.phase==PW_SLEEP);w.mood=50;set_time(&w,"20261108230000");pw_advance(&w,60000);assert(w.mood==53);
    assert(pw_request(&w,PW_REST)==PW_OK);pw_advance(&w,3000);assert(w.phase==PW_IDLE);
    puts("[host:time] civil/UTC+8, protocol, 1440 minute boundaries, activity lock, tired gain and optional sleep PASS");
    return 0;
}
