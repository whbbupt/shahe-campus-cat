/* Run the production adapter against bounded SDK/RT mocks. This is not RF testing. */
#include "../src/pet_ble.c"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
static event_t queue[16];
static unsigned queued,advertising_calls,stop_calls,disconnect_calls,allocations,fail_allocation;
static bool queue_create_fail,thread_create_fail,thread_start_fail,register_fail;
static unsigned advertising_error;
rt_mq_t rt_mq_create(const char *s,size_t n,size_t count,int flags) { (void)s;(void)flags;assert(n==sizeof(event_t)&&count==16);return queue_create_fail?NULL:queue; }
int rt_mq_send(rt_mq_t q,const void *p,size_t n) { assert(q==queue&&n==sizeof(event_t));if(queued==16)return -1;queue[queued++]=*(const event_t *)p;return 0; }
int rt_mq_recv(rt_mq_t q,void *p,size_t n,int timeout) { (void)q;(void)timeout;assert(n==sizeof(event_t));if(!queued)return -1;memcpy(p,queue,n);memmove(queue,queue+1,--queued*n);return 0; }
int rt_mq_delete(rt_mq_t q) { assert(q==queue);return 0; }
rt_thread_t rt_thread_create(const char *n,void (*f)(void *),void *p,unsigned stack,unsigned priority,unsigned slice)
{ (void)n;(void)p;(void)slice;assert(f==task&&stack==4096&&priority==21);return thread_create_fail?NULL:(void *)1; }
int rt_thread_startup(rt_thread_t p) { assert(p);return thread_start_fail?-1:0; }
int rt_thread_delete(rt_thread_t p) { assert(p);return 0; }
rt_tick_t rt_tick_get(void) { return 0; }
rt_tick_t rt_tick_from_millisecond(int ms) { return ms; }
void *rt_malloc(size_t n) { allocations++;return allocations==fail_allocation?NULL:malloc(n); }
void rt_free(void *p) { free(p); }
int rt_snprintf(char *s,size_t n,const char *format,...) { va_list args;va_start(args,format);int r=vsnprintf(s,n,format,args);va_end(args);return r; }
int rt_kprintf(const char *f,...) { (void)f;return 0; }
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
uint8_t sibles_advertising_start(void *p) { assert(p);advertising_calls++;return 0; }
uint8_t sibles_advertising_stop(void *p) { assert(p);stop_calls++;return 0; }
void sifli_ble_enable(void) {}
static void drain(void) { event_t e;while(rt_mq_recv(events,&e,sizeof(e),0)==0)handle(&e); }
static void connect_peer(uint8_t index) { connection_manager_connect_ind_t c={index};stack_event(CONNECTION_MANAGER_CONNCTED_IND,(uint8_t *)&c,sizeof(c),0);drain(); }
int main(void)
{
 assert(attributes[ATTR_SERVICE].uuid[0]==0 && attributes[ATTR_SERVICE].uuid[1]==0x28);
 assert(attributes[ATTR_INFO_CHAR].uuid[0]==3 && attributes[ATTR_INFO].uuid[12]==1);
 assert(attributes[ATTR_PROBE].uuid[12]==2 && attributes[ATTR_PROBE].permission==3);
 queue_create_fail=true;assert(pet_ble_start()!=0&&!events&&!started);queue_create_fail=false;
 thread_create_fail=true;assert(pet_ble_start()!=0&&!events&&!started);thread_create_fail=false;
 thread_start_fail=true;assert(pet_ble_start()!=0&&!events&&!started);thread_start_fail=false;
 assert(pet_ble_start()==0&&pet_ble_start()==0&&queued==0);
 stack_event(BLE_POWER_ON_IND,NULL,0,0);drain();assert(service_ready&&advertising_calls==1);
 connect_peer(0);assert(active_connection==0&&callback_connection==0);
 uint16_t n;assert(read_value(0,ATTR_INFO,&n)&&n>20);
 assert(read_value(0,ATTR_PROBE,&n)&&n==5&&!memcmp(probe_value,"READY",5));
 sibles_set_cbk_t request={ATTR_PROBE,4,0,(uint8_t *)"PING"};
 assert(write_value(0,&request)==0);assert(read_value(0,ATTR_PROBE,&n)&&n==4&&!memcmp(probe_value,"PONG",4));drain();
 request.offset=1;assert(write_value(0,&request)!=0);request.offset=0;
 request.value=(uint8_t *)"NOPE";assert(write_value(0,&request)!=0&&!memcmp(probe_value,"PONG",4));
 request.value=(uint8_t *)"PING";request.len=3;assert(write_value(0,&request)!=0);request.len=4;
 request.idx=ATTR_INFO;assert(write_value(0,&request)!=0);request.idx=ATTR_PROBE;
 assert(write_value(2,&request)!=0);
 connect_peer(1);assert(disconnect_calls==1&&active_connection==0&&callback_connection==0);
 ble_gap_disconnected_ind_t d={0,19};stack_event(BLE_GAP_DISCONNECTED_IND,(uint8_t *)&d,sizeof(d),0);drain();
 assert(active_connection==0xff&&callback_connection==0xff&&probe_length==5);
 assert(write_value(0xff,&request)!=0);connect_peer(2);assert(active_connection==2);
 assert(write_value(2,&request)==0);drain();
 queued=16;assert(write_value(2,&request)!=0&&queue_fault);queued=0;
 stop_faulted_radio(NULL);assert(fault_latched&&stop_calls==1&&disconnect_calls==2);
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
