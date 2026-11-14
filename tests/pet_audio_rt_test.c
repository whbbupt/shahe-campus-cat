#include "pet_audio_rt.h"
#include "rtthread.h"
#include "audio_server.h"
#include "pet_audio.h"
#include <assert.h>
#include <stdio.h>
#include <setjmp.h>
#include <string.h>
static pet_world_t world;
static void (*entry)(void *);
static jmp_buf done;
static unsigned scenario, writes, opens, closes, ticks, locks, volume=7, failures, volume_seen;
static bool live;
static bool no_thread;
static unsigned resumed_tick;
static pa_mixer_t expected;
static int16_t expected_block[160];
static unsigned expected_at;
rt_tick_t rt_tick_get(void) { return ticks; }
rt_tick_t rt_tick_from_millisecond(int ms) { return (rt_tick_t)ms; }
int rt_kprintf(const char *f, ...) { if (strstr(f,"unavailable;")) failures++; return 0; }
int rt_mutex_init(struct rt_mutex *m,const char *n,int flags) { (void)m;(void)n;(void)flags; return 0; }
int rt_mutex_take(struct rt_mutex *m,int t) { (void)m;(void)t; assert(!locks);locks++;return 0; }
int rt_mutex_release(struct rt_mutex *m) { (void)m;assert(locks==1);locks--;return 0; }
rt_thread_t rt_thread_create(const char *n,void (*f)(void *),void *arg,unsigned stack,unsigned priority,unsigned slice)
{
    (void)n;(void)arg;(void)slice;assert(stack>=4096 && priority==18);
    if (no_thread) return NULL;
    entry=f;return &entry;
}
int rt_thread_startup(rt_thread_t t) { assert(t);return 0; }
int rt_thread_delete(rt_thread_t t) { (void)t;return 0; }
uint8_t audio_server_get_private_volume(int t) { (void)t; assert(!locks); return (uint8_t)volume; }
int audio_server_set_private_volume(int t,uint8_t v)
{
    (void)t;assert(!locks); if (scenario==5 && v==5) return -1; volume=v;
    volume_seen |= 1U<<v; return 0;
}
audio_client_t audio_open2(int t,int rw,audio_parameter_t *p,void *cb,void *arg,int device)
{
    (void)t;(void)rw;(void)cb;(void)arg;(void)device;
    assert(!locks && !live && p->write_samplerate==16000 && p->write_bits_per_sample==16);
    assert(p->write_channnel_num==1 && p->write_cache_size==1024);
    if(scenario==4) return NULL;
    opens++;live=true;return &live;
}
int audio_close(audio_client_t c) { assert(c && live && !locks);live=false;closes++;return 0; }
int audio_write(audio_client_t c,uint8_t *data,uint32_t n)
{
    assert(c && live && !locks && n<=320 && !(n&1));writes++;
    if(scenario==1) return 0;
    if(scenario==3) return -1;
    if(scenario==6) return 1;
    if(scenario==7) return (int)n+2;
    unsigned accepted=scenario==2 && n>80 ? 80 : n;
    if (scenario==0 || scenario==2 || scenario==8 || scenario==9 || scenario==10) {
        if (!expected_at) pa_mix(&expected, world.sound_mode, false, expected_block, 160);
        assert(n==320-expected_at && !memcmp(data,(uint8_t *)expected_block+expected_at,n));
        expected_at=(expected_at+accepted)%320;
    }
    memset(data,0,n);
    return (int)accepted;
}
void rt_thread_mdelay(int ms)
{
    assert(!locks);ticks+=(unsigned)ms;
    if (scenario == 12)
    {
        if (writes == 3 && live && !resumed_tick) pet_audio_voice_pause(true);
        if (!live && writes == 3 && !resumed_tick && pet_audio_voice_parked())
        {
            assert(volume == 7 && closes == 1);
            resumed_tick = ticks;
            pet_audio_voice_pause(false);
            assert(!pet_audio_voice_parked());
        }
        if (opens == 2 && writes >= 6 && live)
        {
            world.sound_mode = 0;
            pet_audio_sync(&world);
        }
        if (!live && opens == 2) longjmp(done, 1);
    }
    if (scenario==11) {
        if (!resumed_tick && writes>=3) {
            world.volume=0;pet_audio_sync(&world);
            if(!live) { world.volume=5;pet_audio_sync(&world);resumed_tick=ticks; }
        }
        if(resumed_tick && ticks>=resumed_tick+100) {
            assert(!live && opens==1);longjmp(done,1);
        }
    }
    if (scenario==10) {
        if(writes>=30)world.volume=0;
        else if(writes>=20)world.volume=2;
        else if(writes>=10)world.volume=8;
        pet_audio_sync(&world);
        if(!live && !world.volume)longjmp(done,1);
    }
    if ((scenario==0 || scenario==2) && writes>40 && world.sound_mode)
    { world.sound_mode=0;pet_audio_sync(&world); }
    if(!live && ((scenario==0 || scenario==2) ? !world.sound_mode
                : (scenario==8 || scenario==9) ? closes>0 : failures>0))
        longjmp(done,1);
    assert(ticks<2000);
}
static void run(unsigned which)
{
    scenario=which;writes=opens=closes=ticks=failures=volume_seen=resumed_tick=0;volume=7;world.volume=5;
    memset(&expected,0,sizeof(expected));expected_at=0;
    world.sound_mode=0;pet_audio_sync(&world);
    world.sound_mode=which==8 || which==9 || which==11 ? 1 : 2;pet_audio_sync(&world);
    if(which==8 || which==9 || which==11) {
        world.settlements[PW_TOUCH]++;pet_audio_sync(&world);
        pa_trigger(&expected,PA_TOUCH);
        if(which==9) {
            world.journal_stamps++;pet_audio_sync(&world);
            pa_trigger(&expected,PA_STAMP);
        }
    }
    if(!setjmp(done)) entry(NULL);
    assert(!live && opens==closes && volume==7 && !locks);
    if(which==1) assert(writes>1 && ticks>=300 && !pet_audio_available());
    if(which==0 || which==2) assert(writes>40 && pet_audio_available());
    else if(which==12) assert(writes==6 && opens==2 && pet_audio_available());
    else if(which==11) assert(writes==3 && opens==1 && world.volume==5 && pet_audio_available());
    else if(which==10) assert(writes>=30 && opens==1 && world.volume==0 &&
        (volume_seen&(1U<<8)) && (volume_seen&(1U<<2)) && pet_audio_available());
    else if(which>=8) assert(writes>30 && opens==1 && world.sound_mode==1 && pet_audio_available());
    else assert(!pet_audio_available());
}
int main(int argc, char **argv)
{
    (void)argv;
    no_thread=argc>1;
    pw_init(&world);pet_audio_start(&world);
    if(no_thread) {
        assert(!pet_audio_available());
        world.sound_mode=0;pet_audio_sync(&world);
        world.sound_mode=2;pet_audio_sync(&world);
        assert(!pet_audio_available() && !opens);
        puts("[host:audio-driver] thread creation failure remains unavailable across toggles PASS");
        return 0;
    }
    assert(entry && pet_audio_available() && !live);
    for(unsigned i=0;i<13;i++) run(i);
    puts("[host:audio-driver] live volume/zero, no stale cue on unmute, restored volume, voice handoff and failures PASS (13 cases)");
    return 0;
}
