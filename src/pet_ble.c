/* SDK v2.5.1 example/ble/peripheral and peripheral_lab GATT/advertising APIs.
 * Bluetooth never accesses LVGL, pet state, RTC or pet storage. */
#include "pet_ble.h"
#include "pet_ble_probe.h"
#include "pet_version.h"
#include "bf0_ble_gap.h"
#include "bf0_sibles.h"
#include "bf0_sibles_advertising.h"
#include "ble_connection_manager.h"
#include <rthw.h>
#ifdef RT_USING_FINSH
#include <finsh.h>
#endif
#include <string.h>

#define UUID(n) {0x43,0x45,0x48,0x41,0x48,0x53,0x4d,0x9a,0x25,0x4f,0x73,0x2f,n,0x00,0x7c,0x8a}
#define SERIAL_UUID_16(x) {(uint8_t)((x)&255),(uint8_t)((x)>>8)}
enum { ATTR_SERVICE, ATTR_INFO_CHAR, ATTR_INFO, ATTR_PROBE_CHAR, ATTR_PROBE,
       ATTR_PHONE_CHAR, ATTR_PHONE, ATTR_COUNT };
enum { EVENT_ADV_ON=0xf100, EVENT_ADV_OFF, EVENT_PING };
typedef struct { uint16_t id,length; union { uint32_t align; uint8_t bytes[64]; } data; } event_t;
static rt_mq_t events;
static volatile bool queue_fault;
static bool started, service_ready, fault_latched;
static volatile uint8_t active_connection=0xff;
/* Callbacks own this byte; worker reads it only to disconnect after a queue fault. */
static volatile uint8_t callback_connection=0xff;
static uint8_t probe_value[5]={'R','E','A','D','Y'};
static uint16_t probe_length=5;
static char phone_status[96]="status=pending", phone_read_buffer[96];
static uint16_t phone_status_length=14;
static pet_phone_status_t published_status;
static bool phone_status_ready;
static uint8_t service_uuid[16]=UUID(0);
static uint8_t info[]="ShaheCat " PET_FIRMWARE_VERSION " BLE probe; RTC sync not enabled";
static sibles_hdl service;
SIBLES_ADVERTISING_CONTEXT_DECLAR(advertising);

enum { DIAG_OFF, DIAG_QUEUE, DIAG_THREAD, DIAG_STACK_CALL, DIAG_WAIT_READY,
       DIAG_GATT, DIAG_NAME, DIAG_ADV_INIT, DIAG_ADV_START, DIAG_ADVERTISING,
       DIAG_CONNECTED, DIAG_WAIT_ADV, DIAG_ERROR };
typedef struct {
    uint32_t stage, callbacks, power_events, queued, handled, loops;
    uint32_t last_callback, last_handled, adv_events, adv_result;
    int result, last_receive;
    char name[24];
} diagnostic_t;
static diagnostic_t diagnostic;
static rt_tick_t diagnostic_start_tick;
static bool diagnostic_started, diagnostic_reported;

static const char *stage_name(unsigned stage)
{
    static const char *names[]={"off","queue","thread","stack-call","wait-ready",
        "gatt","name","adv-init","adv-start","advertising","connected","wait-adv","error"};
    return stage<sizeof(names)/sizeof(names[0]) ? names[stage] : "unknown";
}
static void set_stage(unsigned stage,int result)
{
    rt_base_t level=rt_hw_interrupt_disable();
    diagnostic.stage=stage; diagnostic.result=result;
    rt_hw_interrupt_enable(level);
    rt_kprintf("[pet:BLE] stage=%s result=%d\n",stage_name(stage),result);
}
static diagnostic_t snapshot(void)
{
    rt_base_t level=rt_hw_interrupt_disable();
    diagnostic_t copy=diagnostic;
    rt_hw_interrupt_enable(level);
    return copy;
}
void pet_ble_set_phone_status(const pet_phone_status_t *status)
{
    if (!status || status->food>100 || status->mood>100 || status->scene>4 ||
        status->phase>5 || status->volume>10 || status->sound_mode>2) return;
    rt_base_t level=rt_hw_interrupt_disable();
    bool same=phone_status_ready && !memcmp(status,&published_status,sizeof(*status));
    rt_hw_interrupt_enable(level);
    if (same) return;
    static const char *scenes[]={"dorm","classroom","canteen","campus","gym"};
    static const char *phases[]={"idle","walking","activity","sleeping","entering","minigame"};
    char value[96];
    int count=rt_snprintf(value,sizeof(value),"food=%u;mood=%u;scene=%s;phase=%s;volume=%u;music=%s",
        status->food,status->mood,scenes[status->scene],phases[status->phase],
        status->volume,status->sound_mode==2?"on":"off");
    if (count<0 || (unsigned)count>=sizeof(value)) return;
    level=rt_hw_interrupt_disable();
    published_status=*status;phone_status_ready=true;
    memcpy(phone_status,value,(size_t)count+1);phone_status_length=(uint16_t)count;
    rt_hw_interrupt_enable(level);
}
void pet_ble_get_link(pet_link_t *link)
{
    if (!link) return;
    rt_base_t level=rt_hw_interrupt_disable();
    unsigned stage=diagnostic.stage;
    link->state=queue_fault || stage==DIAG_ERROR ? PET_LINK_ERROR
        : active_connection!=0xff ? PET_LINK_CONNECTED
        : stage==DIAG_ADVERTISING || stage==DIAG_WAIT_ADV ? PET_LINK_WAITING
        : PET_LINK_STARTING;
    memcpy(link->name,diagnostic.name,sizeof(link->name));
    rt_hw_interrupt_enable(level);
    link->name[sizeof(link->name)-1]=0;
    if (!link->name[0]) memcpy(link->name,"ShaheCat",9);
}
static void print_status(const char *reason)
{
    diagnostic_t d=snapshot();
    rt_kprintf("[pet:BLE:STATUS] %s version=%s tick=%u stage=%s result=%d\n",
        reason,PET_FIRMWARE_VERSION,(unsigned)rt_tick_get(),stage_name(d.stage),d.result);
    rt_kprintf("[pet:BLE:STATUS] callbacks=%u power=%u queued=%u handled=%u loops=%u\n",
        (unsigned)d.callbacks,(unsigned)d.power_events,(unsigned)d.queued,(unsigned)d.handled,(unsigned)d.loops);
    rt_kprintf("[pet:BLE:STATUS] last_cb=0x%04X last_handled=0x%04X recv=%d adv_events=%u adv_result=%u fault=%u\n",
        (unsigned)d.last_callback,(unsigned)d.last_handled,d.last_receive,
        (unsigned)d.adv_events,(unsigned)d.adv_result,(unsigned)queue_fault);
}
void pet_ble_poll(void)
{
    if (diagnostic_started && !diagnostic_reported &&
        (rt_tick_t)(rt_tick_get()-diagnostic_start_tick)>=rt_tick_from_millisecond(15000)) {
        diagnostic_reported=true;
        print_status("15s");
    }
}
#ifdef RT_USING_FINSH
static int cmd_pet_ble_status(int argc,char **argv)
{
    (void)argc; (void)argv;
    print_status("query");
    return 0;
}
MSH_CMD_EXPORT_ALIAS(cmd_pet_ble_status,pet_ble_status,Read pet BLE diagnostic snapshot);
#endif

BLE_GATT_SERVICE_DEFINE_128(attributes) {
    BLE_GATT_SERVICE_DECLARE(ATTR_SERVICE,SERIAL_UUID_16_PRI_SERVICE,BLE_GATT_PERM_READ_ENABLE),
    BLE_GATT_CHAR_DECLARE(ATTR_INFO_CHAR,SERIAL_UUID_16_CHARACTERISTIC,BLE_GATT_PERM_READ_ENABLE),
    BLE_GATT_CHAR_VALUE_DECLARE(ATTR_INFO,UUID(1),BLE_GATT_PERM_READ_ENABLE,
        BLE_GATT_VALUE_PERM_UUID_128|BLE_GATT_VALUE_PERM_RI_ENABLE,sizeof(info)-1),
    BLE_GATT_CHAR_DECLARE(ATTR_PROBE_CHAR,SERIAL_UUID_16_CHARACTERISTIC,BLE_GATT_PERM_READ_ENABLE),
    BLE_GATT_CHAR_VALUE_DECLARE(ATTR_PROBE,UUID(2),BLE_GATT_PERM_READ_ENABLE|BLE_GATT_PERM_WRITE_REQ_ENABLE,
        BLE_GATT_VALUE_PERM_UUID_128|BLE_GATT_VALUE_PERM_RI_ENABLE,5),
    BLE_GATT_CHAR_DECLARE(ATTR_PHONE_CHAR,SERIAL_UUID_16_CHARACTERISTIC,BLE_GATT_PERM_READ_ENABLE),
    BLE_GATT_CHAR_VALUE_DECLARE(ATTR_PHONE,UUID(3),BLE_GATT_PERM_READ_ENABLE,
        BLE_GATT_VALUE_PERM_UUID_128|BLE_GATT_VALUE_PERM_RI_ENABLE,95),
};

static void reset_probe(void) { memcpy(probe_value,"READY",5); probe_length=5; }
static bool enqueue(uint16_t id,const void *data,unsigned length)
{
    if (!events || length>sizeof(((event_t *)0)->data.bytes) || (length && !data)) return false;
    event_t event={0}; event.id=id; event.length=(uint16_t)length;
    if (length) memcpy(event.data.bytes,data,length);
    if (rt_mq_send(events,&event,sizeof(event))!=RT_EOK) { queue_fault=true; return false; }
    rt_base_t level=rt_hw_interrupt_disable(); diagnostic.queued++; rt_hw_interrupt_enable(level);
    return true;
}
static uint8_t *read_value(uint8_t connection,uint8_t attribute,uint16_t *length)
{
    *length=0;
    if (queue_fault || callback_connection==0xff || connection!=callback_connection) return NULL;
    if (attribute==ATTR_INFO) { *length=sizeof(info)-1; return info; }
    if (attribute==ATTR_PROBE) { *length=probe_length; return probe_value; }
    if (attribute==ATTR_PHONE) {
        rt_base_t level=rt_hw_interrupt_disable();
        *length=phone_status_length;memcpy(phone_read_buffer,phone_status,(size_t)*length+1);
        rt_hw_interrupt_enable(level);
        return (uint8_t *)phone_read_buffer;
    }
    return NULL;
}
static uint8_t write_value(uint8_t connection,sibles_set_cbk_t *request)
{
    uint8_t reply[4];
    if (queue_fault || callback_connection==0xff || connection!=callback_connection) return 0x0e;
    if (!request || request->idx!=ATTR_PROBE) return 0x03;
    if (request->offset) return 0x07;
    if (request->len!=4) return 0x0d;
    if (!pet_ble_probe_reply(request->value,request->len,reply)) return 0x13;
    if (!enqueue(EVENT_PING,&connection,1)) return 0x11;
    memcpy(probe_value,reply,4); probe_length=4;
    return 0;
}
static uint8_t advertising_event(uint8_t id,void *context,void *data)
{
    (void)context;
    if (!data) { queue_fault=true; return 0; }
    if (id==SIBLES_ADV_EVT_ADV_STARTED) {
        uint8_t status=((sibles_adv_evt_startted_t *)data)->status;
        if (!enqueue(EVENT_ADV_ON,&status,1)) queue_fault=true;
    } else if (id==SIBLES_ADV_EVT_ADV_STOPPED) {
        uint8_t reason=((sibles_adv_evt_stopped_t *)data)->reason;
        if (!enqueue(EVENT_ADV_OFF,&reason,1)) queue_fault=true;
    }
    return 0;
}
static int stack_event(uint16_t id,uint8_t *data,uint16_t length,uint32_t context)
{
    (void)context;
    rt_base_t level=rt_hw_interrupt_disable();
    diagnostic.callbacks++; diagnostic.last_callback=id;
    if (id==BLE_POWER_ON_IND) diagnostic.power_events++;
    rt_hw_interrupt_enable(level);
    if (!events) return 0;
    unsigned needed=0;
    if (id==BLE_POWER_ON_IND) { if (!enqueue(id,NULL,0)) queue_fault=true; return 0; }
    if (id==CONNECTION_MANAGER_CONNCTED_IND) needed=sizeof(connection_manager_connect_ind_t);
    else if (id==BLE_GAP_DISCONNECTED_IND) needed=sizeof(ble_gap_disconnected_ind_t);
    else return 0;
    if (!data || length<needed || needed>sizeof(((event_t *)0)->data.bytes)) { queue_fault=true; return 0; }
    if (id==CONNECTION_MANAGER_CONNCTED_IND) {
        connection_manager_connect_ind_t *p=(void *)data;
        if (callback_connection==0xff) { callback_connection=p->conn_idx; reset_probe(); }
    } else {
        ble_gap_disconnected_ind_t *p=(void *)data;
        if (p->conn_idx==callback_connection) { callback_connection=0xff; reset_probe(); }
    }
    if (!enqueue(id,data,needed)) queue_fault=true;
    return 0;
}
BLE_EVENT_REGISTER(stack_event,0);

static void disconnect(uint8_t connection)
{
    ble_gap_disconnect_t request={0}; request.conn_idx=connection; request.reason=CO_ERROR_REMOTE_USER_TERM_CON;
    unsigned result=ble_gap_disconnect(&request);
    if (result) rt_kprintf("[pet:BLE] disconnect failed=%u\n",result);
}
static bool register_service(void)
{
    set_stage(DIAG_GATT,0);
    BLE_GATT_SERVICE_INIT_128(definition,attributes,ATTR_COUNT,
        BLE_GATT_SERVICE_PERM_NOAUTH|BLE_GATT_SERVICE_PERM_UUID_128,service_uuid);
    service=sibles_register_svc_128(&definition);
    if (!service) { set_stage(DIAG_ERROR,-100); rt_kprintf("[pet:BLE] GATT registration failed\n"); return false; }
    sibles_register_cbk(service,read_value,write_value);
    char name[24]="ShaheCat";
    bd_addr_t address;
    if (!ble_get_public_address(&address))
        rt_snprintf(name,sizeof(name),"ShaheCat-%02X%02X%02X",address.addr[2],address.addr[1],address.addr[0]);
    rt_base_t level=rt_hw_interrupt_disable();
    memcpy(diagnostic.name,name,sizeof(name));
    rt_hw_interrupt_enable(level);
    unsigned length=strlen(name);
    ble_gap_dev_name_t *device_name=rt_malloc(sizeof(*device_name)+length);
    sibles_adv_type_name_t *scan_name=rt_malloc(sizeof(*scan_name)+length);
    sibles_adv_type_srv_uuid_t *uuid=rt_malloc(sizeof(*uuid)+sizeof(sibles_adv_uuid_t));
    if (!device_name || !scan_name || !uuid) {
        rt_free(device_name); rt_free(scan_name); rt_free(uuid);
        set_stage(DIAG_ERROR,-RT_ENOMEM);
        rt_kprintf("[pet:BLE] advertising allocation failed\n"); return false;
    }
    device_name->len=length; memcpy(device_name->name,name,length);
    set_stage(DIAG_NAME,0);
    unsigned result=ble_gap_set_dev_name(device_name);
    scan_name->name_len=length; memcpy(scan_name->name,name,length);
    uuid->count=1; uuid->uuid_list[0].uuid_len=16;
    memcpy(uuid->uuid_list[0].uuid.uuid_128,service_uuid,16);
    sibles_advertising_para_t parameters={0};
    parameters.own_addr_type=GAPM_STATIC_ADDR;
    parameters.config.adv_mode=SIBLES_ADV_CONNECT_MODE;
    parameters.config.mode_config.conn_config.duration=0;
    parameters.config.mode_config.conn_config.interval=160;
    parameters.config.max_tx_pwr=0x7f;
    parameters.config.is_auto_restart=1;
    parameters.rsp_data.completed_name=scan_name;
    parameters.adv_data.completed_uuid=uuid;
    parameters.evt_handler=advertising_event;
    if (!result) { set_stage(DIAG_ADV_INIT,0); result=sibles_advertising_init(advertising,&parameters); }
    rt_free(device_name); rt_free(scan_name); rt_free(uuid);
    if (!result) { set_stage(DIAG_ADV_START,0); result=sibles_advertising_start(advertising); }
    if (result) { set_stage(DIAG_ERROR,(int)result); rt_kprintf("[pet:BLE] advertising setup failed=%u\n",result); return false; }
    rt_kprintf("[pet:BLE] name=%s; open LightBlue, scan and connect\n",name);
    return true;
}
static void handle(const event_t *event)
{
    rt_base_t level=rt_hw_interrupt_disable();
    diagnostic.handled++; diagnostic.last_handled=event->id;
    rt_hw_interrupt_enable(level);
    if (event->id==BLE_POWER_ON_IND) {
        if (!service_ready) service_ready=register_service();
    } else if (event->id==CONNECTION_MANAGER_CONNCTED_IND) {
        const connection_manager_connect_ind_t *p=(const void *)event->data.bytes;
        if (active_connection!=0xff || !service_ready) { disconnect(p->conn_idx); return; }
        active_connection=p->conn_idx;
        set_stage(DIAG_CONNECTED,0);
        rt_kprintf("[pet:BLE] connected index=%u; link only, read/write not verified\n",p->conn_idx);
    } else if (event->id==BLE_GAP_DISCONNECTED_IND) {
        const ble_gap_disconnected_ind_t *p=(const void *)event->data.bytes;
        if (active_connection==p->conn_idx) active_connection=0xff;
        set_stage(DIAG_WAIT_ADV,0);
        rt_kprintf("[pet:BLE] disconnected reason=%u; SDK restarts advertising\n",p->reason);
    } else if (event->id==EVENT_ADV_ON) {
        level=rt_hw_interrupt_disable();
        diagnostic.adv_events++; diagnostic.adv_result=event->data.bytes[0];
        rt_hw_interrupt_enable(level);
        set_stage(event->data.bytes[0] ? DIAG_ERROR : DIAG_ADVERTISING,event->data.bytes[0]);
        rt_kprintf("[pet:BLE] advertising result=%u\n",event->data.bytes[0]);
    } else if (event->id==EVENT_ADV_OFF) {
        rt_kprintf("[pet:BLE] advertising stopped reason=%u\n",event->data.bytes[0]);
    } else if (event->id==EVENT_PING) {
        rt_kprintf("[pet:BLE] PING accepted index=%u; read probe to verify PONG on phone\n",event->data.bytes[0]);
    }
}
static void stop_faulted_radio(const event_t *event)
{
    if (!fault_latched) {
        fault_latched=true;
        set_stage(DIAG_ERROR,-101);
        if (service_ready) sibles_advertising_stop(advertising);
        if (active_connection!=0xff) disconnect(active_connection);
        uint8_t pending_connection=callback_connection;
        if (pending_connection!=0xff && pending_connection!=active_connection) disconnect(pending_connection);
        rt_kprintf("[pet:BLE] event fault; restart board before testing; pet continues\n");
    }
    /* SDK auto-restart can complete after the first stop/disconnect request. */
    if (event && event->id==EVENT_ADV_ON && !event->data.bytes[0] && service_ready)
        sibles_advertising_stop(advertising);
    if (event && event->id==CONNECTION_MANAGER_CONNCTED_IND) {
        const connection_manager_connect_ind_t *p=(const void *)event->data.bytes;
        disconnect(p->conn_idx);
    }
}
static void task(void *unused)
{
    (void)unused;
    set_stage(DIAG_STACK_CALL,0);
    sifli_ble_enable();
    set_stage(DIAG_WAIT_READY,0);
    rt_tick_t startup=rt_tick_get();
    bool timeout_reported=false;
    for (;;) {
        event_t event;
        rt_err_t received=rt_mq_recv(events,&event,sizeof(event),rt_tick_from_millisecond(100));
        rt_base_t level=rt_hw_interrupt_disable();
        diagnostic.loops++; diagnostic.last_receive=received;
        rt_hw_interrupt_enable(level);
        if (queue_fault) stop_faulted_radio(received==RT_EOK ? &event : NULL);
        else if (received==RT_EOK) handle(&event);
        if (!service_ready && !timeout_reported &&
            (rt_tick_t)(rt_tick_get()-startup)>=rt_tick_from_millisecond(15000)) {
            timeout_reported=true;
            rt_kprintf("[pet:BLE] startup not ready; check initialization log; pet continues\n");
        }
    }
}
rt_err_t pet_ble_start(void)
{
    if (started) return RT_EOK;
    diagnostic_start_tick=rt_tick_get(); diagnostic_started=true; diagnostic_reported=false;
    set_stage(DIAG_QUEUE,0);
    if (!pet_ble_probe_selftest()) { rt_kprintf("[pet:BLE-TEST] protocol FAIL\n"); return -RT_ERROR; }
    rt_kprintf("[pet:BLE-TEST] protocol PASS (RAM only, no radio verdict)\n");
    events=rt_mq_create("pet_ble",sizeof(event_t),16,RT_IPC_FLAG_FIFO);
    if (!events) { set_stage(DIAG_ERROR,-RT_ENOMEM); return -RT_ENOMEM; }
    rt_thread_t thread=rt_thread_create("pet_ble",task,NULL,4096,21,10);
    if (!thread) { rt_mq_delete(events); events=NULL; set_stage(DIAG_ERROR,-RT_ENOMEM); return -RT_ENOMEM; }
    set_stage(DIAG_THREAD,0);
    rt_err_t result=rt_thread_startup(thread);
    if (result!=RT_EOK) { rt_thread_delete(thread); rt_mq_delete(events); events=NULL; set_stage(DIAG_ERROR,result); return result; }
    started=true;
    return RT_EOK;
}
