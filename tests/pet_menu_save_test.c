#include "pet_save.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static uint8_t bytes[PS_AREA_SIZE],baseline[PS_AREA_SIZE];static int budget=-1;
static bool rd(void *c,uint32_t o,uint8_t *p,size_t n){(void)c;assert(o+n<=sizeof(bytes));memcpy(p,bytes+o,n);return true;}
static bool wr(void *c,uint32_t o,const uint8_t *p,size_t n)
{
    (void)c;assert(o+n<=sizeof(bytes));
    for(size_t i=0;i<n;i++){if(budget==0)return false;if(budget>0)budget--;assert((bytes[o+i]&p[i])==p[i]);bytes[o+i]&=p[i];}return true;
}
static bool er(void *c,uint32_t o,size_t n){(void)c;assert(o+n<=sizeof(bytes));memset(bytes+o,255,n);return true;}
int main(void)
{
    FILE *f=fopen("actual-v9-nor.bin","rb");assert(f&&fread(bytes,sizeof(bytes),1,f)==1);fclose(f);
    pet_save_t s;pet_world_t w;pw_init(&w);ps_io_t io={NULL,rd,wr,er};
    assert(ps_open(&s,io,&w)==PS_OK && w.wallet.coins==123 && w.checkin_day==20000);
    assert(w.order==PW_ORDER_DELIVERING && !w.order_choice && !strcmp(pe_order(0)->name,"暖饭便当"));
    pw_advance(&w,12000);assert(w.order==PW_ORDER_READY);
    assert(ps_save(&s,&w)==PS_OK && s.saved[4]==10);
    assert(ps_open(&s,io,&w)==PS_OK && w.order==PW_ORDER_READY && !w.order_choice && w.wallet.coins==123);
    w.order=PW_ORDER_NONE;assert(ps_save(&s,&w)==PS_OK);memcpy(baseline,bytes,sizeof(bytes));
    for(unsigned dish=0;dish<PE_MENU_COUNT;dish++)
    {
        const pe_food_t *food=pe_menu(true,dish);
        for(int cut=0;cut<=64;cut++)
        {
            memcpy(bytes,baseline,sizeof(bytes));assert(ps_open(&s,io,&w)==PS_OK);
            assert(pw_request(&w,(pw_command_t)(PW_DELIVERY_FIRST+dish))==PW_OK);
            assert(w.wallet.coins==123-food->price && w.order_choice==dish+1 && !ps_visible_saved(&s,&w));
            budget=cut;ps_result_t result=ps_save(&s,&w);budget=-1;
            assert(ps_open(&s,io,&w)==PS_OK && w.wallet.owned==7 && w.wallet.equipped==5 && w.checkin_day==20000);
            if(result==PS_OK) assert(w.wallet.coins==123-food->price && w.order==PW_ORDER_DELIVERING && w.order_choice==dish+1);
            else assert(w.wallet.coins==123 && w.order==PW_ORDER_NONE && !w.order_choice);
        }
        pw_advance(&w,PW_DELIVERY_MS);assert(w.order==PW_ORDER_READY);
        assert(ps_save(&s,&w)==PS_OK && ps_open(&s,io,&w)==PS_OK && w.order_choice==dish+1);
        assert(pw_request(&w,PW_TAKEOUT)==PW_OK);pw_advance(&w,20000);
        assert(w.order==PW_ORDER_NONE && !w.order_choice && w.food==42+food->food && w.wallet.coins==123-food->price);
        w.order_choice=7;assert(ps_save(&s,&w)==PS_INVALID);
        w.order_choice=1;assert(ps_save(&s,&w)==PS_INVALID);
    }
    memcpy(bytes,baseline,sizeof(bytes));assert(ps_open(&s,io,&w)==PS_OK);
    assert(pw_request(&w,PW_TOUCH)==PW_OK);pw_advance(&w,1500);
    assert(ps_save(&s,&w)==PS_OK);assert(ps_open(&s,io,&w)==PS_OK && w.cooldown[PW_TOUCH]==PW_TOUCH_COOLDOWN_MS);
    assert(pw_request(&w,PW_TOUCH)==PW_COOLDOWN);pw_advance(&w,PW_TOUCH_COOLDOWN_MS);assert(pw_request(&w,PW_TOUCH)==PW_OK);
    puts("[host:menu-save] actual9 legacy meal migration; six kinds x65 transaction cuts; ready/reboot/pickup; invalid kinds PASS");return 0;
}
