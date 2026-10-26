#include "pet_world.h"
#include <math.h>
#define CHECK(x) do { if(!(x))return false; } while(0)
static bool arrive(pet_world_t *w){for(unsigned i=0;i<3000&&w->phase==PW_WALK;i++)pw_advance(w,10);return w->phase==PW_ACTION;}
static void travel(pet_world_t *w,pw_scene_t s){pw_request(w,(pw_command_t)(PW_GO_DORM+s));pw_advance(w,20000);}
static bool initial(void){pet_world_t w;pw_init(&w);CHECK(w.food==70&&w.mood==60&&w.scene==PW_DORM&&w.phase==PW_IDLE);CHECK(pw_request(&w,PW_EAT)==PW_WRONG_ROOM);CHECK(w.food==70);return true;}
static bool deferred(void){pet_world_t w;pw_init(&w);CHECK(pw_request(&w,PW_CODE)==PW_OK);CHECK(w.phase==PW_WALK&&w.food==70&&w.settlements[PW_CODE]==0);CHECK(arrive(&w));CHECK(w.food==67&&w.mood==64&&w.settlements[PW_CODE]==1);return true;}
static bool retarget(void){pet_world_t w;pw_init(&w);pw_request(&w,PW_CODE);pw_advance(&w,300);float x=w.x,y=w.y;CHECK(pw_request(&w,PW_REST)==PW_OK);CHECK(w.x==x&&w.y==y&&w.food==70);CHECK(arrive(&w));CHECK(w.action==PW_REST&&w.settlements[PW_CODE]==0);return true;}
static bool arrival_recheck(void){pet_world_t w;pw_init(&w);w.food=8;pw_request(&w,PW_CODE);w.food=7;for(int i=0;i<1000&&w.phase==PW_WALK;i++)pw_advance(&w,10);CHECK(w.phase==PW_IDLE&&w.food==7&&w.settlements[PW_CODE]==0);return true;}
static bool no_spam(void){pet_world_t w;pw_init(&w);pw_request(&w,PW_CODE);CHECK(arrive(&w));uint8_t f=w.food,m=w.mood;for(int i=0;i<30;i++)CHECK(pw_request(&w,PW_CODE)==PW_BUSY);CHECK(w.food==f&&w.mood==m&&w.settlements[PW_CODE]==1);CHECK(pw_request(&w,PW_GO_CLASSROOM)==PW_BUSY);return true;}
static bool cooldown(void){pet_world_t w;pw_init(&w);pw_request(&w,PW_CODE);CHECK(arrive(&w));pw_advance(&w,(uint32_t)(w.action_end-w.now));CHECK(w.phase==PW_IDLE&&w.cooldown[PW_CODE]==w.now+10000);CHECK(pw_request(&w,PW_CODE)==PW_COOLDOWN);pw_advance(&w,10000);CHECK(pw_request(&w,PW_CODE)==PW_OK);return true;}
static bool capped_meal(void){pet_world_t w;pw_init(&w);travel(&w,PW_CANTEEN);CHECK(w.scene==PW_CANTEEN&&w.food==70&&w.mood==60);w.food=96;pw_request(&w,PW_EAT);CHECK(arrive(&w));CHECK(w.food==100&&w.gain==4);pw_advance(&w,4000);CHECK(pw_request(&w,PW_EAT)==PW_COOLDOWN);pw_advance(&w,2000);CHECK(pw_request(&w,PW_EAT)==PW_FULL);return true;}
static bool high_mood_study(void){pet_world_t w;pw_init(&w);travel(&w,PW_CLASSROOM);w.mood=100;w.food=10;CHECK(pw_request(&w,PW_STUDY)==PW_OK);CHECK(arrive(&w));CHECK(w.food==5&&w.mood==100);CHECK(pw_duration(PW_STUDY)==8000);return true;}
static bool sleep_wake(void){pet_world_t w;pw_init(&w);pw_request(&w,PW_REST);CHECK(arrive(&w));pw_advance(&w,600);CHECK(w.phase==PW_SLEEP);pw_visual_t v;pw_visual(&w,&v);CHECK(v.frame==18&&fabsf(v.x-62)<.01f&&fabsf(v.y-195)<.01f);CHECK(pw_request(&w,PW_GO_CANTEEN)==PW_ASLEEP);pw_advance(&w,60000);CHECK(w.mood==61);CHECK(pw_request(&w,PW_REST)==PW_OK);pw_advance(&w,500);CHECK(w.phase==PW_IDLE&&w.x==120&&w.y==268);return true;}
static bool touch_cancel(void){pet_world_t w;pw_init(&w);pw_request(&w,PW_CODE);pw_advance(&w,100);float x=w.x,y=w.y;CHECK(pw_request(&w,PW_TOUCH)==PW_OK);CHECK(w.x==x&&w.y==y&&w.mood==70&&w.settlements[PW_CODE]==0);pw_advance(&w,1500);CHECK(w.phase==PW_IDLE&&w.cooldown[PW_TOUCH]==w.now+3000);return true;}
static bool elapsed_partition(void){pet_world_t a,b;pw_init(&a);pw_init(&b);a.phase=b.phase=PW_SLEEP;pw_advance(&a,3600000);for(int i=0;i<3600;i++)pw_advance(&b,1000);CHECK(a.food==b.food&&a.mood==b.mood&&a.rest_elapsed==b.rest_elapsed&&a.food_elapsed==b.food_elapsed);pw_init(&a);pw_advance(&a,86400000);CHECK(a.food==0&&a.mood==0);return true;}
static bool jump_support(void){pet_world_t w;pw_init(&w);pw_request(&w,PW_CODE);CHECK(arrive(&w));pw_advance(&w,250);pw_visual_t v;pw_visual(&w,&v);CHECK(v.airborne&&v.frame<16&&v.lift>16);pw_advance(&w,300);pw_visual(&w,&v);CHECK(v.frame>=16&&v.frame<=17&&v.occlusion&&fabsf(v.y-210)<.01f);pw_advance(&w,(uint32_t)(w.action_end-w.now-250));pw_visual(&w,&v);CHECK(v.airborne&&v.frame<16);pw_advance(&w,250);CHECK(w.phase==PW_IDLE&&w.x==303&&w.y==277);return true;}
static bool all_directions(void){pet_world_t w;pw_init(&w);unsigned seen=0;for(int i=0;i<9000;i++){pw_advance(&w,40);pw_visual_t v;pw_visual(&w,&v);if(w.phase==PW_WALK)seen|=1U<<v.facing;if(i==700)pw_request(&w,PW_REST);if(w.phase==PW_SLEEP)pw_request(&w,PW_REST);if(i==2000)pw_request(&w,PW_CODE);}CHECK(seen==255);return true;}
static bool reset_invalid(void){pet_world_t w;pw_init(&w);CHECK(pw_request(&w,(pw_command_t)99)==PW_INVALID);w.food=0;w.mood=100;pw_init(&w);CHECK(w.food==70&&w.mood==60&&w.phase==PW_IDLE);return true;}
bool pw_selftest(void (*report)(const char *,bool)){
 static const struct{const char *name;bool(*run)(void);}cases[]={
 {"initial-scene",initial},{"arrive-once",deferred},{"retarget-no-snap",retarget},{"arrival-eligibility",arrival_recheck},
 {"busy-no-queue",no_spam},{"cooldown-from-finish",cooldown},{"capped-meal",capped_meal},{"full-mood-study",high_mood_study},
 {"sleep-wake-support",sleep_wake},{"touch-cancels-intent",touch_cancel},{"elapsed-and-long-gap",elapsed_partition},
 {"jump-and-seat",jump_support},{"eight-directions",all_directions},{"reset-invalid",reset_invalid}};
 bool all=true;for(unsigned i=0;i<sizeof(cases)/sizeof(cases[0]);i++){bool ok=cases[i].run();if(report)report(cases[i].name,ok);all=all&&ok;}return all;
}
