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
    memset(bytes,255,sizeof(bytes));pet_world_t w;pw_init(&w);
    w.wallet=(pe_wallet_t){123,7,5};w.food=42;w.mood=38;w.checkin_day=20000;
    w.order=PW_ORDER_DELIVERING;w.order_due=12000;
    pet_save_t s;ps_io_t io={NULL,rd,wr,er};assert(ps_open(&s,io,&w)==PS_EMPTY);
    assert(ps_save(&s,&w)==PS_OK && s.saved[4]==9);
    FILE *f=fopen("actual-v9-nor.bin","wb");assert(f&&fwrite(bytes,sizeof(bytes),1,f)==1);fclose(f);
    puts("[host:save-old9] actual a01b11f source produces schema9 order/wallet/furniture/checkin fixture PASS");return 0;
}
