/* SDK v2.5.1 bf0_ble_tipc.c API pattern, with bounded callback copies and failures. */
#include "pet_cts.h"
#include "pet_time.h"
#include "bf0_sibles.h"
#include "bf0_ble_gap.h"
#include "ble_connection_manager.h"
#include <rtthread.h>
#include <rthw.h>
#include <string.h>
enum { OFF,SEARCH,PAIR,REGISTER,ZONE,TIME_VALUE,QUEUED,FAILED };
enum { DISCOVERED=1,REGISTERED,READ_VALUE,READ_ERROR,SECURED,PAIR_FAILED,SERVICE_CHANGED };
typedef struct {
    uint32_t epoch;
    rt_tick_t received_tick;
    uint8_t type,connection,status,length;
    uint16_t start,end,time,zone,handle;
    uint8_t value[10];
} message_t;
static struct {
    uint32_t epoch;
    uint8_t connection,phase;
    bool live,pair_attempted,encrypted,overflow,refresh,rediscovered;
    uint16_t remote,start,end,time,zone;
    rt_tick_t phase_tick;
    uint8_t local[2];
} client={.connection=0xff,.remote=SIBLES_ERROR_REMOTE_HANDLE};
static message_t messages[8];
static unsigned head,count;
static pet_cts_submit_t submit_time;
static int remote_event(uint16_t id,uint8_t *data,uint16_t length);
static const char *const names[]={"idle","discover","pairing","register","read-zone","read-time","sample-queued","unavailable"};
const char *pet_cts_stage(void)
{
    rt_base_t lock=rt_hw_interrupt_disable();unsigned phase=client.phase;rt_hw_interrupt_enable(lock);
    return names[phase];
}
unsigned pet_cts_status(void)
{
    rt_base_t lock=rt_hw_interrupt_disable();unsigned phase=client.phase;rt_hw_interrupt_enable(lock);
    return phase==OFF?0:phase==FAILED?2:phase==QUEUED?3:1;
}
static void phase(unsigned next,const char *reason)
{
    rt_base_t lock=rt_hw_interrupt_disable();client.phase=(uint8_t)next;client.phase_tick=rt_tick_get();rt_hw_interrupt_enable(lock);
    rt_kprintf("[pet:CTS] stage=%s reason=%s\n",names[next],reason);
}
static void release(void)
{
    if(client.remote!=SIBLES_ERROR_REMOTE_HANDLE){
        sibles_unregister_remote_svc(client.connection,client.start,client.end,remote_event);
        client.remote=SIBLES_ERROR_REMOTE_HANDLE;
    }
}
void pet_cts_init(pet_cts_submit_t submit) { submit_time=submit; }
void pet_cts_invalidate(uint8_t connection)
{
    rt_base_t lock=rt_hw_interrupt_disable();
    if(client.connection==connection){client.live=false;client.epoch++;}
    rt_hw_interrupt_enable(lock);
}
void pet_cts_disconnected(uint8_t connection)
{
    if(client.connection!=connection)return;
    pet_cts_invalidate(connection);release();
    rt_base_t lock=rt_hw_interrupt_disable();client.connection=0xff;client.phase=OFF;head=count=0;rt_hw_interrupt_enable(lock);
}
static bool live(void)
{
    rt_base_t lock=rt_hw_interrupt_disable();bool value=client.live;rt_hw_interrupt_enable(lock);return value;
}
static void search(void)
{
    uint8_t uuid[]={5,0x18};phase(SEARCH,"cts");
    if(sibles_search_service(client.connection,2,uuid))phase(FAILED,"search-request");
}
void pet_cts_connected(uint8_t connection)
{
    pet_cts_disconnected(client.connection);
    rt_base_t lock=rt_hw_interrupt_disable();uint32_t epoch=client.epoch+1;
    memset(&client,0,sizeof(client));client.epoch=epoch;client.connection=connection;client.live=true;client.remote=SIBLES_ERROR_REMOTE_HANDLE;
    head=count=0;rt_hw_interrupt_enable(lock);
    search();
}
static void retry_pair(const char *reason)
{
    if(!live())return;
    release();
    if(!client.pair_attempted && !client.encrypted){
        client.pair_attempted=true;phase(PAIR,reason);connection_manager_create_bond(client.connection);
    }else phase(FAILED,reason);
}
static bool begin_message(message_t *m,uint8_t connection,uint8_t type)
{
    rt_base_t lock=rt_hw_interrupt_disable();bool ok=client.live && connection==client.connection;
    if(ok){memset(m,0,sizeof(*m));m->epoch=client.epoch;m->received_tick=rt_tick_get();m->connection=connection;m->type=type;}
    rt_hw_interrupt_enable(lock);return ok;
}
static void push(const message_t *m)
{
    rt_base_t lock=rt_hw_interrupt_disable();
    if(client.live && m->epoch==client.epoch){
        if(count==8)client.overflow=true;
        else {messages[(head+count)%8]=*m;count++;}
    }
    rt_hw_interrupt_enable(lock);
}
static void read_handle(uint16_t handle,unsigned next)
{
    if(!live())return;
    sibles_read_remote_value_req_t request={0};request.read_type=SIBLES_READ;request.handle=handle;
    phase(next,"request");
    if(sibles_read_remote_value(client.remote,client.connection,&request))retry_pair("read-request");
}
static int remote_event(uint16_t id,uint8_t *data,uint16_t length)
{
    message_t m;
    if(id==SIBLES_REGISTER_REMOTE_SVC_RSP && data && length>=sizeof(sibles_register_remote_svc_rsp_t)){
        sibles_register_remote_svc_rsp_t *r=(void *)data;
        if(begin_message(&m,r->conn_idx,REGISTERED)){m.status=r->status;push(&m);}
    }else if(id==SIBLES_READ_REMOTE_VALUE_RSP && data && length>=sizeof(sibles_read_remote_value_rsp_t)){
        sibles_read_remote_value_rsp_t *r=(void *)data;
        if(begin_message(&m,r->conn_idx,READ_VALUE)){
            m.handle=r->handle;
            if(!r->offset && r->value && r->length<=10){m.length=(uint8_t)r->length;memcpy(m.value,r->value,r->length);}
            push(&m);
        }
    }
    return 0;
}
void pet_cts_event(uint16_t id,uint8_t *data,uint16_t length)
{
    message_t m;
    if(!data)return;
    if(id==SIBLES_SEARCH_SVC_RSP && length>=sizeof(sibles_svc_search_rsp_t)){
        sibles_svc_search_rsp_t *r=(void *)data;
        if(r->search_svc_len!=2 || r->search_uuid[0]!=5 || r->search_uuid[1]!=0x18 || !begin_message(&m,r->conn_idx,DISCOVERED))return;
        sibles_svc_remote_svc_t *svc=r->svc;
        m.status=1;
        if(!r->result && svc && svc->att_db && svc->char_count>0 && svc->char_count<=16 && svc->hdl_start && svc->hdl_start<svc->hdl_end){
            m.start=svc->hdl_start;m.end=svc->hdl_end;
            const uint8_t *next=svc->att_db;
            for(unsigned i=0;i<svc->char_count;i++){
                const sibles_svc_search_char_t *c=(const void *)next;
                if(c->desc_count>16){m.time=m.zone=0;break;}
                if(c->uuid_len==2 && (c->prop&2) && c->pointer_hdl>m.start && c->pointer_hdl<=m.end){
                    if(c->uuid[0]==0x2b && c->uuid[1]==0x2a)m.time=c->pointer_hdl;
                    if(c->uuid[0]==0x0f && c->uuid[1]==0x2a)m.zone=c->pointer_hdl;
                }
                next+=sizeof(*c)+(size_t)c->desc_count*sizeof(struct sibles_disc_char_desc_ind);
            }
            if(m.time && m.zone && m.time!=m.zone)m.status=0;
        }
        push(&m);
    }else if(id==BLE_GAP_ENCRYPT_IND && length>=sizeof(ble_gap_encrypt_ind_t)){
        ble_gap_encrypt_ind_t *r=(void *)data;if(begin_message(&m,r->conn_idx,SECURED))push(&m);
    }else if(id==BLE_GAP_BOND_IND && length>=sizeof(ble_gap_bond_ind_t)){
        ble_gap_bond_ind_t *r=(void *)data;
        unsigned event=r->info==GAPC_PAIRING_SUCCEED?SECURED:r->info==GAPC_PAIRING_FAILED?PAIR_FAILED:0;
        if(event && begin_message(&m,r->conn_idx,(uint8_t)event))push(&m);
    }else if(id==SIBLES_READ_REMOTE_VALUE_ERROR_IND && length>=sizeof(sibles_error_cmp_event_ind_t)){
        sibles_error_cmp_event_ind_t *r=(void *)data;if(begin_message(&m,r->conn_idx,READ_ERROR))push(&m);
    }else if(id==SIBLES_REMOTE_SVC_CHANGE_IND && length>=sizeof(sibles_remote_svc_change_ind_t)){
        sibles_remote_svc_change_ind_t *r=(void *)data;
        if(begin_message(&m,r->conn_idx,SERVICE_CHANGED)){m.start=r->start_handle;m.end=r->end_handle;push(&m);}
    }
}
void pet_cts_poll(void)
{
    if(!live())return;
    rt_base_t lock=rt_hw_interrupt_disable();bool overflow=client.overflow;client.overflow=false;rt_hw_interrupt_enable(lock);
    if(overflow){release();phase(FAILED,"queue-full");return;}
    for(unsigned guard=0;guard<8;guard++){
        message_t m;lock=rt_hw_interrupt_disable();
        if(!count){rt_hw_interrupt_enable(lock);break;}
        m=messages[head];head=(head+1)%8;count--;
        bool valid=client.live && m.epoch==client.epoch;rt_hw_interrupt_enable(lock);
        if(!valid)continue;
        if(m.type==SECURED){
            client.encrypted=true;
            if(client.phase==PAIR){client.refresh=false;search();}
            else if(client.phase==SEARCH)client.refresh=true;
        }
        else if(m.type==PAIR_FAILED && client.phase==PAIR)phase(FAILED,"pair-rejected");
        else if(m.type==DISCOVERED && client.phase==SEARCH){
            if(m.status){
                if(client.refresh && !client.rediscovered){client.refresh=false;client.rediscovered=true;search();}
                else retry_pair("cts-unavailable");
                continue;
            }
            client.start=m.start;client.end=m.end;client.time=m.time;client.zone=m.zone;
            phase(REGISTER,"cts-found");
            client.remote=sibles_register_remote_svc(client.connection,m.start,m.end,remote_event);
            if(client.remote==SIBLES_ERROR_REMOTE_HANDLE)phase(FAILED,"remote-slots-full");
        }else if(m.type==REGISTERED && client.phase==REGISTER){
            if(m.status)retry_pair("register-error");else read_handle(client.zone,ZONE);
        }else if(m.type==READ_ERROR && (client.phase==ZONE || client.phase==TIME_VALUE))retry_pair("read-error");
        else if(m.type==READ_VALUE && client.phase==ZONE && m.handle==client.zone){
            if(m.length!=2){release();phase(FAILED,"zone-length");continue;}
            memcpy(client.local,m.value,2);read_handle(client.time,TIME_VALUE);
        }else if(m.type==READ_VALUE && client.phase==TIME_VALUE && m.handle==client.time){
            uint32_t utc;
            bool decoded=pt_from_cts(m.value,m.length,client.local,2,&utc);
            release();
            if(decoded && live() && submit_time && submit_time(utc,m.received_tick))phase(QUEUED,"main-thread-pending");
            else phase(FAILED,decoded?"sample-not-accepted":"invalid-date-zone");
        }else if(m.type==SERVICE_CHANGED){
            if(client.phase==SEARCH)client.refresh=true;
            else if(client.phase!=PAIR && client.phase!=OFF && client.phase!=FAILED &&
                    m.start<=client.end && m.end>=client.start){release();phase(FAILED,"service-changed-reconnect");}
        }
    }
    unsigned state=client.phase;
    if(state>=SEARCH && state<=TIME_VALUE && (rt_tick_t)(rt_tick_get()-client.phase_tick)>=rt_tick_from_millisecond(state==PAIR?30000:8000)){
        if(state==SEARCH)retry_pair("discover-timeout");
        else {release();phase(FAILED,"timeout");}
    }
}
