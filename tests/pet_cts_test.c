/* Actual production CTS adapter with native SDK mocks. No radio/RTC access. */
#include "../src/pet_cts.c"
#include <assert.h>
#include <stdio.h>
static rt_tick_t tick;
static rt_base_t irq;
static unsigned searches,bonds,registers,reads,unregisters,submits,malformed;
static uint16_t last_read;
static bool search_error,read_error,register_error,reject_submit;
static uint32_t submitted,received;
static sibles_remote_svc_cbk callback;
rt_tick_t rt_tick_get(void){return tick;}
rt_tick_t rt_tick_from_millisecond(int ms){return (rt_tick_t)ms;}
rt_base_t rt_hw_interrupt_disable(void){return irq++;}
void rt_hw_interrupt_enable(rt_base_t old){assert(irq==old+1);irq=old;}
int rt_kprintf(const char *fmt,...){(void)fmt;assert(!irq);return 0;}
int8_t sibles_search_service(uint8_t c,uint8_t n,uint8_t *u){assert(c==0 && n==2 && u[0]==5 && u[1]==0x18);searches++;return search_error?1:0;}
void connection_manager_create_bond(uint8_t c){assert(c==0);bonds++;}
uint16_t sibles_register_remote_svc(uint8_t c,uint16_t start,uint16_t end,sibles_remote_svc_cbk cb){assert(!irq && c==0 && start==10 && end==30);callback=cb;registers++;return register_error?0xffff:0;}
void sibles_unregister_remote_svc(uint8_t c,uint16_t start,uint16_t end,sibles_remote_svc_cbk cb){assert(!irq && c==0 && start==10 && end==30 && cb==callback);unregisters++;}
int8_t sibles_read_remote_value(uint16_t remote,uint8_t c,sibles_read_remote_value_req_t *v){assert(!irq && remote==0 && c==0 && v->read_type==SIBLES_READ && !v->offset && !v->length);reads++;last_read=v->handle;return read_error?1:0;}
static bool submit(uint32_t utc,uint32_t observed){assert(!irq);submits++;submitted=utc;received=observed;return !reject_submit;}
static void reset(void){pet_cts_disconnected(0);tick=0;searches=bonds=registers=reads=unregisters=submits=0;search_error=read_error=register_error=reject_submit=false;malformed=0;pet_cts_init(submit);pet_cts_connected(0);}
static void discover(bool found){
    union { uint32_t align;uint8_t bytes[2*sizeof(sibles_svc_search_char_t)]; } chars={0};
    sibles_svc_search_char_t *c=(void *)chars.bytes;
    c->pointer_hdl=14;c->prop=2;c->uuid_len=2;c->uuid[0]=0x2b;c->uuid[1]=0x2a;
    c=(void *)(chars.bytes+sizeof(*c));
    c->pointer_hdl=18;c->prop=2;c->uuid_len=2;c->uuid[0]=0x0f;c->uuid[1]=0x2a;
    sibles_svc_remote_svc_t svc={0};svc.hdl_start=10;svc.hdl_end=30;svc.char_count=2;svc.att_db=chars.bytes;
    sibles_svc_search_rsp_t r={0};r.search_svc_len=2;r.search_uuid[0]=5;r.search_uuid[1]=0x18;r.svc=found?&svc:NULL;
    if(malformed==1)svc.att_db=NULL;
    if(malformed==2)svc.char_count=17;
    if(malformed==3)((sibles_svc_search_char_t *)chars.bytes)->pointer_hdl=0;
    if(malformed==4)r.search_uuid[0]=4;
    if(malformed==5)r.conn_idx=1;
    if(malformed==6)((sibles_svc_search_char_t *)chars.bytes)->desc_count=17;
    pet_cts_event(SIBLES_SEARCH_SVC_RSP,(uint8_t *)&r,sizeof(r));
    memset(&chars,0,sizeof(chars)); /* callback must not retain SDK pointers */
    pet_cts_poll();
}
static void registered(unsigned error){sibles_register_remote_svc_rsp_t r={0,(uint8_t)error};callback(SIBLES_REGISTER_REMOTE_SVC_RSP,(uint8_t *)&r,sizeof(r));pet_cts_poll();}
static void value(uint16_t handle,uint8_t *v,unsigned n){sibles_read_remote_value_rsp_t r={0};r.handle=handle;r.length=(uint16_t)n;r.value=v;callback(SIBLES_READ_REMOTE_VALUE_RSP,(uint8_t *)&r,sizeof(r));}
static void secure(void){ble_gap_encrypt_ind_t r={0,1};pet_cts_event(BLE_GAP_ENCRYPT_IND,(uint8_t *)&r,sizeof(r));pet_cts_poll();}
static void ready(void){reset();discover(true);assert(registers==1);registered(0);assert(last_read==18);uint8_t zone[]={32,0};value(18,zone,2);pet_cts_poll();assert(last_read==14);}
int main(void){
    uint8_t stamp[]={0xea,7,11,8,23,0,0,7,0,0},zone[]={32,0};uint32_t utc;
    assert(pt_from_cts(stamp,10,zone,2,&utc));uint32_t expected;assert(pt_parse((const uint8_t *)"20261108230000",14,&expected)&&utc==expected);
    zone[0]=(uint8_t)-20;zone[1]=4;assert(pt_from_cts(stamp,10,zone,2,&utc)&&utc==expected+12*3600);
    zone[0]=32;zone[1]=0;assert(!pt_from_cts(stamp,9,zone,2,&utc));assert(!pt_from_cts(stamp,10,zone,1,&utc));
    zone[0]=128;assert(!pt_from_cts(stamp,10,zone,2,&utc));zone[0]=32;zone[1]=255;assert(!pt_from_cts(stamp,10,zone,2,&utc));zone[1]=0;
    stamp[7]=8;assert(!pt_from_cts(stamp,10,zone,2,&utc));stamp[7]=7;stamp[9]=16;assert(!pt_from_cts(stamp,10,zone,2,&utc));stamp[9]=0;
    stamp[2]=2;stamp[3]=29;assert(!pt_from_cts(stamp,10,zone,2,&utc));stamp[0]=0xec;assert(pt_from_cts(stamp,10,zone,2,&utc));stamp[0]=0xea;stamp[2]=11;stamp[3]=8;
    for(unsigned kind=1;kind<=6;kind++){
        reset();malformed=kind;discover(true);
        assert(!registers&&!submits);
        if(kind==4 || kind==5)assert(!bonds);else assert(bonds==1);
    }
    reset();discover(true);registered(0);uint8_t short_zone[]={32};value(18,short_zone,1);pet_cts_poll();assert(pet_cts_status()==2&&!submits);
    ready();sibles_error_cmp_event_ind_t read_failure={0,5};pet_cts_event(SIBLES_READ_REMOTE_VALUE_ERROR_IND,(uint8_t *)&read_failure,sizeof(read_failure));pet_cts_poll();assert(bonds==1&&unregisters==1);
    reset();discover(false);ble_gap_bond_ind_t pair_ok={0,GAPC_PAIRING_SUCCEED};pet_cts_event(BLE_GAP_BOND_IND,(uint8_t *)&pair_ok,sizeof(pair_ok));pet_cts_poll();assert(searches==2);
    ready();tick=123;value(14,stamp,10);stamp[4]=1;pet_cts_poll();assert(submits==1&&submitted==expected&&received==123&&pet_cts_status()==3&&unregisters==1);stamp[4]=23;
    reset();discover(false);assert(bonds==1&&pet_cts_status()==1);
    sibles_remote_svc_change_ind_t changed={0};pet_cts_event(SIBLES_REMOTE_SVC_CHANGE_IND,(uint8_t *)&changed,sizeof(changed));pet_cts_poll();assert(pet_cts_status()==1);
    secure();assert(searches==2);secure();assert(searches==2);discover(false);assert(searches==3);discover(false);assert(pet_cts_status()==2&&bonds==1);
    reset();tick=8000;pet_cts_poll();assert(bonds==1);tick+=30000;pet_cts_poll();assert(pet_cts_status()==2&&bonds==1);
    reset();discover(false);ble_gap_bond_ind_t denied={0,GAPC_PAIRING_FAILED};pet_cts_event(BLE_GAP_BOND_IND,(uint8_t *)&denied,sizeof(denied));pet_cts_poll();assert(pet_cts_status()==2);
    reset();register_error=true;discover(true);assert(pet_cts_status()==2&&reads==0);
    reset();discover(true);registered(1);assert(bonds==1&&unregisters==1);
    reset();discover(true);read_error=true;registered(0);assert(bonds==1&&unregisters==1);
    ready();tick=8000;pet_cts_poll();assert(pet_cts_status()==2&&unregisters==1);
    ready();value(14,stamp,9);pet_cts_poll();assert(pet_cts_status()==2&&submits==0);
    ready();value(14,stamp,10);pet_cts_invalidate(0);pet_cts_poll();assert(submits==0);pet_cts_disconnected(0);assert(pet_cts_status()==0&&unregisters==1);
    ready();value(14,stamp,10);pet_cts_invalidate(0);pet_cts_connected(0);pet_cts_poll();assert(submits==0&&pet_cts_status()==1);
    ready();for(unsigned i=0;i<9;i++)value(14,stamp,10);pet_cts_poll();assert(pet_cts_status()==2&&submits==0);
    ready();reject_submit=true;value(14,stamp,10);pet_cts_poll();assert(pet_cts_status()==2&&submits==1);
    reset();pet_cts_disconnected(0);search_error=true;pet_cts_connected(0);assert(pet_cts_status()==2);
    reset();discover(true);pet_cts_event(SIBLES_REMOTE_SVC_CHANGE_IND,(uint8_t *)&changed,sizeof(changed));pet_cts_poll();assert(pet_cts_status()==2&&unregisters==1);
    puts("[host:cts] actual SDK adapter: timezone/DST, pointer copies, zero remote handle, pairing once, timeout, errors, reconnect and overflow PASS");return 0;
}
