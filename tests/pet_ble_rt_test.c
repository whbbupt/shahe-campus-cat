/* Run the production adapter against bounded SDK/RT mocks. This is not RF testing. */
#include "../src/pet_ble.c"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <setjmp.h>
static event_t queue[16];
static unsigned queued,advertising_calls,stop_calls,disconnect_calls,allocations,fail_allocation;
static bool queue_create_fail,thread_create_fail,thread_start_fail,register_fail;
static unsigned advertising_error;
static rt_tick_t tick;
static rt_base_t irq_level;
static unsigned worker_mode,receives,formats;
static jmp_buf worker_exit;
static char printed[32768];
static size_t printed_length;
rt_mq_t rt_mq_create(const char *s,size_t n,size_t count,int flags) { (void)s;(void)flags;assert(n==sizeof(event_t)&&count==16);return queue_create_fail?NULL:queue; }
int rt_mq_send(rt_mq_t q,const void *p,size_t n) { assert(q==queue&&n==sizeof(event_t));if(queued==16)return -1;queue[queued++]=*(const event_t *)p;return 0; }
int rt_mq_recv(rt_mq_t q,void *p,size_t n,int timeout) {
 (void)q;(void)timeout;assert(n==sizeof(event_t));
 if(worker_mode) { if(receives++==4)longjmp(worker_exit,1);tick+=100; }
 if(!queued)return -2;
 memcpy(p,queue,n);memmove(queue,queue+1,--queued*n);return 0;
}
int rt_mq_delete(rt_mq_t q) { assert(q==queue);return 0; }
rt_thread_t rt_thread_create(const char *n,void (*f)(void *),void *p,unsigned stack,unsigned priority,unsigned slice)
{ (void)n;(void)p;(void)slice;assert(f==task&&stack==4096&&priority==21);return thread_create_fail?NULL:(void *)1; }
int rt_thread_startup(rt_thread_t p) { assert(p);return thread_start_fail?-1:0; }
int rt_thread_delete(rt_thread_t p) { assert(p);return 0; }
rt_tick_t rt_tick_get(void) { return tick; }
rt_tick_t rt_tick_from_millisecond(int ms) { return ms; }
rt_base_t rt_hw_interrupt_disable(void) { return irq_level++; }
void rt_hw_interrupt_enable(rt_base_t previous) { assert(irq_level==previous+1);irq_level=previous; }
void *rt_malloc(size_t n) { allocations++;return allocations==fail_allocation?NULL:malloc(n); }
void rt_free(void *p) { free(p); }
int rt_snprintf(char *s,size_t n,const char *format,...) { formats++;assert(!irq_level);va_list args;va_start(args,format);int r=vsnprintf(s,n,format,args);va_end(args);return r; }
int rt_kprintf(const char *f,...) {
 assert(irq_level==0);
 va_list args;va_start(args,f);int result=vsnprintf(printed+printed_length,sizeof(printed)-printed_length,f,args);va_end(args);
 assert(result>=0&&(size_t)result<sizeof(printed)-printed_length);printed_length+=(size_t)result;return result;
}
sibles_hdl sibles_register_svc_128(void *p) { assert(p);return register_fail?NULL:(void *)1; }
void sibles_register_cbk(sibles_hdl h,uint8_t *(*read)(uint8_t,uint8_t,uint16_t *),uint8_t (*write)(uint8_t,sibles_set_cbk_t *)) { assert(h&&read==read_value&&write==write_value); }
uint8_t ble_get_public_address(bd_addr_t *p) { memset(p->addr,0x12,6);return 0; }
uint8_t ble_gap_set_dev_name(ble_gap_dev_name_t *p) { assert(p->len==15&&!memcmp(p->name,"ShaheCat-121212",15));return 0; }
uint8_t ble_gap_disconnect(ble_gap_disconnect_t *p) { assert(p->reason==CO_ERROR_REMOTE_USER_TERM_CON);disconnect_calls++;return 0; }
uint8_t sibles_advertising_init(void *p,sibles_advertising_para_t *a) {
 assert(p&&a->config.is_auto_restart&&a->config.mode_config.conn_config.interval==160);
 assert(a->rsp_data.completed_name->name_len==15&&a->adv_data.completed_uuid->count==1);
 assert(!memcmp(a->adv_data.completed_uuid->uuid_list[0].uuid.uuid_128,service_uuid,16));
 return advertising_error;
}
uint8_t sibles_advertising_start(void *p) {
 assert(p);advertising_calls++;
 if(worker_mode) { sibles_adv_evt_startted_t event={(uint8_t)(worker_mode==3?12:0)};advertising_event(SIBLES_ADV_EVT_ADV_STARTED,NULL,&event); }
 return 0;
}
uint8_t sibles_advertising_stop(void *p) { assert(p);stop_calls++;return 0; }
void sifli_ble_enable(void) {
 if(!worker_mode)return;
 assert(snapshot().stage==DIAG_STACK_CALL);
 /* Simulate a slow SDK call: the UI watchdog remains independently callable. */
 tick=14999;pet_ble_poll();assert(!diagnostic_reported);
 tick=15000;pet_ble_poll();assert(diagnostic_reported&&strstr(printed,"15s version="));
 assert(strstr(printed,"stage=stack-call"));
 if(worker_mode==4)queued=16;
 if(worker_mode!=2)stack_event(BLE_POWER_ON_IND,NULL,0,0);
}
int8_t sibles_search_service(uint8_t c,uint8_t n,uint8_t *uuid) { (void)c;assert(n==2 && uuid[0]==5 && uuid[1]==0x18);return 0; }
uint16_t sibles_register_remote_svc(uint8_t c,uint16_t a,uint16_t b,sibles_remote_svc_cbk cb) { (void)c;(void)a;(void)b;(void)cb;return 0; }
void sibles_unregister_remote_svc(uint8_t c,uint16_t a,uint16_t b,sibles_remote_svc_cbk cb) { (void)c;(void)a;(void)b;(void)cb; }
int8_t sibles_read_remote_value(uint16_t h,uint8_t c,sibles_read_remote_value_req_t *v) { (void)h;(void)c;(void)v;return 0; }
void connection_manager_create_bond(uint8_t c) { (void)c; }
uint8_t connection_manager_set_link_security(uint8_t c,uint8_t level) { (void)c;assert(level==LE_SECURITY_LEVEL_SEC_CON_BOND);return CM_STATUS_OK; }
static void drain(void) { event_t e;while(rt_mq_recv(events,&e,sizeof(e),0)==0)handle(&e); }
static void connect_peer(uint8_t index) { connection_manager_connect_ind_t c={index};stack_event(CONNECTION_MANAGER_CONNCTED_IND,(uint8_t *)&c,sizeof(c),0);drain(); }
int main(int argc,char **argv)
{
 pet_link_t link;
 pet_ble_get_link(&link);assert(link.state==PET_LINK_STARTING&&!strcmp(link.name,"ShaheCat"));
 if(argc>1) {
  worker_mode=(unsigned)atoi(argv[1]);assert(worker_mode>=1&&worker_mode<=4);
  assert(pet_ble_start()==0);
  if(!setjmp(worker_exit))task(NULL);
  diagnostic_t before=snapshot();
  assert(before.loops==4);
  if(worker_mode==1)assert(before.stage==DIAG_ADVERTISING&&before.power_events==1&&before.queued==2&&before.handled==2);
  if(worker_mode==2)assert(before.stage==DIAG_WAIT_READY&&before.callbacks==0&&before.handled==0);
  if(worker_mode==3)assert(before.stage==DIAG_ERROR&&before.adv_result==12&&before.result==12);
  if(worker_mode==4)assert(queue_fault&&fault_latched&&before.stage==DIAG_ERROR);
  unsigned old_advertising=advertising_calls,old_disconnect=disconnect_calls;size_t old_printed=printed_length;
  pet_ble_poll();assert(printed_length==old_printed);
  assert(cmd_pet_ble_status(0,NULL)==0);diagnostic_t after=snapshot();
  assert(!memcmp(&before,&after,sizeof(before))&&advertising_calls==old_advertising&&disconnect_calls==old_disconnect);
  assert(strstr(printed,"query version="));
  printf("[host:ble-worker] mode=%u production loop, event flow, independent 15s watchdog and read-only query PASS; no radio verdict\n",worker_mode);
  return 0;
 }
 assert(attributes[ATTR_SERVICE].uuid[0]==0 && attributes[ATTR_SERVICE].uuid[1]==0x28);
 assert(attributes[ATTR_INFO_CHAR].uuid[0]==3 && attributes[ATTR_INFO].uuid[12]==1);
 assert(attributes[ATTR_PROBE].uuid[12]==2 && attributes[ATTR_PROBE].permission==3);
 assert(attributes[ATTR_TIME].uuid[12]==4 && attributes[ATTR_TIME].permission==3);
 assert(attributes[ATTR_PHONE].uuid[12]==3 && attributes[ATTR_PHONE].permission==1);
 queue_create_fail=true;assert(pet_ble_start()!=0&&!events&&!started);queue_create_fail=false;
 thread_create_fail=true;assert(pet_ble_start()!=0&&!events&&!started);thread_create_fail=false;
 thread_start_fail=true;assert(pet_ble_start()!=0&&!events&&!started);thread_start_fail=false;
 assert(pet_ble_start()==0&&pet_ble_start()==0&&queued==0);
 stack_event(BLE_POWER_ON_IND,NULL,0,0);drain();assert(service_ready&&advertising_calls==1);
 sibles_adv_evt_startted_t adv={0};advertising_event(SIBLES_ADV_EVT_ADV_STARTED,NULL,&adv);drain();
 pet_ble_get_link(&link);assert(link.state==PET_LINK_WAITING&&strstr(link.name,"ShaheCat-")==link.name);
 connect_peer(0);assert(active_connection==0&&callback_connection==0);
 pet_ble_get_link(&link);assert(link.state==PET_LINK_CONNECTED);
 uint16_t n;assert(read_value(0,ATTR_INFO,&n)&&n>20);
 assert(read_value(0,ATTR_PHONE,&n)&&n==14&&!memcmp(phone_read_buffer,"status=pending",14));
 pet_phone_status_t pet={70,60,0,0,5,2};
 pet_ble_set_phone_status(&pet);
 assert(read_value(0,ATTR_PHONE,&n)&&strstr(phone_read_buffer,"food=70;mood=60;scene=dorm;phase=idle;volume=5;music=on"));
 unsigned previous_formats=formats;pet_ble_set_phone_status(&pet);assert(formats==previous_formats);
 pet.food=82;pet.scene=3;pet.phase=1;pet.volume=7;pet.sound_mode=1;pet_ble_set_phone_status(&pet);
 assert(read_value(0,ATTR_PHONE,&n)&&strstr(phone_read_buffer,"food=82;mood=60;scene=campus;phase=walking;volume=7;music=off"));
 pet.food=101;pet_ble_set_phone_status(&pet);
 assert(read_value(0,ATTR_PHONE,&n)&&strstr(phone_read_buffer,"food=82;"));
 assert(!read_value(2,ATTR_PHONE,&n)&&n==0);
 sibles_set_cbk_t time_write={ATTR_TIME,14,0,(uint8_t *)"20261108230000"};
 assert(write_value(0,&time_write)==0);
 assert(read_value(0,ATTR_TIME,&n)&&n==13&&!memcmp(clock_read_buffer,"state=pending",13));
 assert(write_value(0,&time_write)!=0);
 pet_time_request_t tr;assert(pet_ble_take_time_request(&tr));
 assert(!pet_ble_take_time_request(&tr));
 pt_clock_t clock={tr.utc,true,true,PT_SYNCED,PT_OK};pet_ble_set_clock(&clock);
 pet_ble_finish_time_request(tr.token);
 assert(read_value(0,ATTR_TIME,&n)&&strstr(clock_read_buffer,"2026-11-08 23:00:00;state=synced;result=ok"));
 unsigned clock_formats=formats;pet_ble_set_clock(&clock);assert(formats==clock_formats);
 time_write.offset=1;assert(write_value(0,&time_write)!=0);time_write.offset=0;
 time_write.len=13;assert(write_value(0,&time_write)!=0);time_write.len=14;
 time_write.value=(uint8_t *)"20260229230000";assert(write_value(0,&time_write)!=0&&!pet_ble_take_time_request(&tr));
 time_write.value=(uint8_t *)"20261108230000";
 assert(offer_automatic_time(clock.utc,tick));tick+=1500;
 pet_time_request_t automatic;assert(pet_ble_take_time_request(&automatic)&&automatic.automatic&&automatic.utc==clock.utc+1);
 pet_ble_finish_time_request(automatic.token);
 assert(offer_automatic_time(clock.utc,tick));tick+=2000;assert(!pet_ble_take_time_request(&automatic)&&automatic_expired);
 assert(!offer_automatic_time(clock.utc,tick-2000));
 assert(offer_automatic_time(clock.utc,tick));assert(write_value(0,&time_write)==0);
 assert(pet_ble_take_time_request(&automatic)&&!automatic.automatic);pet_ble_finish_time_request(automatic.token);
 assert(write_value(0,&time_write)==0);assert(!offer_automatic_time(clock.utc,tick));
 assert(pet_ble_take_time_request(&automatic)&&!automatic.automatic);pet_ble_finish_time_request(automatic.token);
 clock.automatic=true;pet_ble_set_clock(&clock);assert(read_value(0,ATTR_TIME,&n)&&strstr(clock_read_buffer,"source=cts"));
 assert(read_value(0,ATTR_PROBE,&n)&&n==5&&!memcmp(probe_value,"READY",5));
 sibles_set_cbk_t request={ATTR_PROBE,4,0,(uint8_t *)"PING"};
 assert(write_value(0,&request)==0);assert(read_value(0,ATTR_PROBE,&n)&&n==4&&!memcmp(probe_value,"PONG",4));drain();
 request.offset=1;assert(write_value(0,&request)!=0);request.offset=0;
 request.value=(uint8_t *)"NOPE";assert(write_value(0,&request)!=0&&!memcmp(probe_value,"PONG",4));
 request.value=(uint8_t *)"PING";request.len=3;assert(write_value(0,&request)!=0);request.len=4;
 request.idx=ATTR_INFO;assert(write_value(0,&request)!=0);request.idx=ATTR_PROBE;
 request.idx=ATTR_PHONE;assert(write_value(0,&request)!=0);request.idx=ATTR_PROBE;
 assert(write_value(2,&request)!=0);
 connect_peer(1);assert(disconnect_calls==1&&active_connection==0&&callback_connection==0);
 assert(write_value(0,&time_write)==0);
 ble_gap_disconnected_ind_t d={0,19};stack_event(BLE_GAP_DISCONNECTED_IND,(uint8_t *)&d,sizeof(d),0);drain();
 assert(active_connection==0xff&&callback_connection==0xff&&probe_length==5);
 assert(!pet_ble_take_time_request(&tr));
 assert(!read_value(0,ATTR_PHONE,&n)&&n==0);
 pet_ble_get_link(&link);assert(link.state==PET_LINK_WAITING);
 assert(write_value(0xff,&request)!=0);connect_peer(2);assert(active_connection==2);
 assert(read_value(2,ATTR_PHONE,&n)&&strstr(phone_read_buffer,"food=82;"));
 assert(write_value(2,&time_write)==0);pet_ble_finish_time_request(tr.token);assert(time_busy);
 assert(pet_ble_take_time_request(&tr));pet_ble_finish_time_request(tr.token);assert(!time_busy);
 assert(write_value(2,&request)==0);drain();
 queued=16;assert(write_value(2,&request)!=0&&queue_fault);queued=0;
 stop_faulted_radio(NULL);assert(fault_latched&&stop_calls==1&&disconnect_calls==2);
 pet_ble_get_link(&link);assert(link.state==PET_LINK_ERROR);
 event_t late={0};late.id=EVENT_ADV_ON;stop_faulted_radio(&late);assert(stop_calls==2);
 late.id=CONNECTION_MANAGER_CONNCTED_IND;late.data.bytes[0]=3;stop_faulted_radio(&late);assert(disconnect_calls==3);
 assert(read_value(2,ATTR_PROBE,&n)==NULL&&write_value(2,&request)!=0);queue_fault=false;
 stack_event(CONNECTION_MANAGER_CONNCTED_IND,NULL,0,0);assert(queue_fault);queue_fault=false;
 register_fail=true;assert(!register_service());register_fail=false;
 for(unsigned i=1;i<=3;++i) { allocations=0;fail_allocation=i;assert(!register_service()); }
 allocations=fail_allocation=0;advertising_error=1;assert(!register_service());
 puts("[host:ble-driver] production callbacks: init failures, identity, connect/read/write/reconnect, invalid offset/peer/data, queue fault and allocation failures PASS; no physical BLE verdict");
 return 0;
}
