#include "pet_save.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static uint8_t bytes[PS_AREA_SIZE];
static bool rd(void *c,uint32_t o,uint8_t *p,size_t n){(void)c;memcpy(p,bytes+o,n);return true;}
static bool wr(void *c,uint32_t o,const uint8_t *p,size_t n){(void)c;for(size_t i=0;i<n;i++)bytes[o+i]&=p[i];return true;}
static bool er(void *c,uint32_t o,size_t n){(void)c;memset(bytes+o,255,n);return true;}
int main(void)
{
    memset(bytes,255,sizeof(bytes)); pet_world_t w;pw_init(&w);
    w.food=42;w.mood=38;w.scene=PW_GYM;w.sound_mode=1;w.volume=4;w.clock.trusted=true;
    w.order=PW_ORDER_DELIVERING;w.order_due=12000;w.cooldown[PW_MEMORY]=45000;
    w.memory_wins=11;w.journal_page=3;w.journal_stamps=9;w.journal_progress=5;w.journal_badges=7;
    ps_io_t io={NULL,rd,wr,er};pet_save_t s;assert(ps_open(&s,io,&w)==PS_EMPTY);
    assert(ps_save(&s,&w)==PS_OK&&s.saved[4]==7);
    FILE *f=fopen("actual-v7-nor.bin","wb");assert(f&&fwrite(bytes,sizeof(bytes),1,f)==1);fclose(f);
    puts("[host:save-old7] actual df6e0db source produces schema7 life/order/RTC/journal fixture PASS");return 0;
}
