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
    FILE *f=fopen("actual-v7-nor.bin","rb");assert(f&&fread(bytes,sizeof(bytes),1,f)==1);fclose(f);
    ps_io_t io={NULL,rd,wr,er};pet_save_t s;pet_world_t w;pw_init(&w);
    assert(ps_open(&s,io,&w)==PS_OK&&w.food==42&&w.mood==38&&w.scene==PW_GYM);
    assert(w.wallet.coins==30&&!w.wallet.owned&&w.memory_wins==11&&w.volume==4&&w.clock.trusted);
    assert(w.order==PW_ORDER_DELIVERING&&w.order_due==12000&&w.cooldown[PW_MEMORY]==45000);
    assert(ps_save(&s,&w)==PS_OK&&s.saved[4]==8);
    w.wallet=(pe_wallet_t){0,7,5};assert(ps_save(&s,&w)==PS_OK);
    for(unsigned i=0;i<3;i++)assert(ps_open(&s,io,&w)==PS_OK&&w.wallet.coins==0&&w.wallet.owned==7&&w.wallet.equipped==5);
    w.wallet.coins=10000;assert(ps_save(&s,&w)==PS_INVALID);
    w.wallet=(pe_wallet_t){30,0,1};assert(ps_save(&s,&w)==PS_INVALID);
    memset(bytes,255,sizeof(bytes));pw_init(&w);assert(ps_open(&s,io,&w)==PS_EMPTY&&ps_save(&s,&w)==PS_OK);
    memcpy(baseline,bytes,sizeof(bytes));
    for(int cut=0;cut<=64;cut++)
    {
        memcpy(bytes,baseline,sizeof(bytes));assert(ps_open(&s,io,&w)==PS_OK);
        w.wallet=(pe_wallet_t){77,3,1};w.memory_wins=12;
        budget=cut;ps_result_t result=ps_save(&s,&w);budget=-1;
        assert(ps_open(&s,io,&w)==PS_OK);
        if(result==PS_OK)assert(w.wallet.coins==77&&w.wallet.owned==3&&w.wallet.equipped==1&&w.memory_wins==12);
        else assert(w.wallet.coins==30&&!w.wallet.owned&&!w.wallet.equipped&&!w.memory_wins);
    }
    puts("[host:economy-save] real schema7 migration, no repeated starter grant, invalid wallet rejection,65 transaction cut points PASS");return 0;
}
