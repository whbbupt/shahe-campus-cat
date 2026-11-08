#include "pet_save.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static uint8_t bytes[PS_AREA_SIZE], baseline[PS_AREA_SIZE];static int budget=-1;
static bool rd(void *c,uint32_t o,uint8_t *p,size_t n){(void)c;assert(o+n<=sizeof(bytes));memcpy(p,bytes+o,n);return true;}
static bool wr(void *c,uint32_t o,const uint8_t *p,size_t n)
{
    (void)c;assert(o+n<=sizeof(bytes));
    for(size_t i=0;i<n;i++){if(budget==0)return false;if(budget>0)budget--;assert((bytes[o+i]&p[i])==p[i]);bytes[o+i]&=p[i];}return true;
}
static bool er(void *c,uint32_t o,size_t n){(void)c;assert(o+n<=sizeof(bytes));memset(bytes+o,255,n);return true;}
int main(void)
{
    FILE *f=fopen("actual-v8-nor.bin","rb");assert(f&&fread(bytes,sizeof(bytes),1,f)==1);fclose(f);
    ps_io_t io={NULL,rd,wr,er};pet_save_t s;pet_world_t w;pw_init(&w);
    assert(ps_open(&s,io,&w)==PS_OK&&w.wallet.coins==123&&w.wallet.owned==7&&w.wallet.equipped==5&&!w.checkin_day);
    assert(w.food==42&&w.mood==38&&w.memory_wins==11&&w.volume==4&&w.clock.trusted);
    memcpy(baseline,bytes,sizeof(bytes));pt_date_t date={2026,11,8,12,0,0};uint32_t utc;assert(pt_encode(&date,&utc));
    for(int cut=0;cut<=64;cut++)
    {
        memcpy(bytes,baseline,sizeof(bytes));assert(ps_open(&s,io,&w)==PS_OK);
        w.clock=(pt_clock_t){utc,true,true,PT_SYNCED,PT_OK};
        assert(pw_request(&w,PW_DAILY_CHECKIN)==PW_OK&&w.wallet.coins==129&&w.checkin_day);
        assert(!ps_visible_saved(&s,&w));
        budget=cut;ps_result_t result=ps_save(&s,&w);budget=-1;
        assert(ps_open(&s,io,&w)==PS_OK&&w.wallet.owned==7&&w.wallet.equipped==5);
        if(result==PS_OK)assert(w.wallet.coins==129&&w.checkin_day);
        else assert(w.wallet.coins==123&&!w.checkin_day);
    }
    w.clock=(pt_clock_t){utc,true,true,PT_SYNCED,PT_OK};
    assert(pw_request(&w,PW_DAILY_CHECKIN)==PW_COOLDOWN&&w.wallet.coins==129);
    assert(ps_visible_saved(&s,&w));
    w.checkin_day=1;assert(ps_save(&s,&w)==PS_INVALID);
    assert(ps_open(&s,io,&w)==PS_OK&&w.wallet.coins==129);
    w.clock=(pt_clock_t){utc+86400,true,true,PT_SYNCED,PT_OK};
    assert(pw_request(&w,PW_DAILY_CHECKIN)==PW_OK&&w.wallet.coins==135);
    assert(ps_poll(&s,&w)==PS_WAIT);pw_advance(&w,3000);assert(ps_poll(&s,&w)==PS_OK);
    assert(ps_open(&s,io,&w)==PS_OK&&w.wallet.coins==135&&w.checkin_day);
    puts("[host:daily-save] actual8 migration,65 coin/day transaction cuts, same-day reboot refusal, next-day autosave and invalid date PASS");return 0;
}
