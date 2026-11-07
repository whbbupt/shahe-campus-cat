/* SDK v2.5.1 example/ble/peripheral and peripheral_lab GATT/advertising APIs.
 * Bluetooth never accesses LVGL, pet state, RTC or pet storage. */
#include "pet_ble.h"
#include "pet_ble_probe.h"
#include "pet_version.h"
#include "bf0_ble_gap.h"
#include "bf0_sibles.h"
#include "bf0_sibles_advertising.h"
#include "ble_connection_manager.h"
#include <string.h>

#define UUID(n) {0x43,0x45,0x48,0x41,0x48,0x53,0x4d,0x9a,0x25,0x4f,0x73,0x2f,n,0x00,0x7c,0x8a}
#define SERIAL_UUID_16(x) {(uint8_t)((x)&255),(uint8_t)((x)>>8)}
enum { ATTR_SERVICE, ATTR_INFO_CHAR, ATTR_INFO, ATTR_PROBE_CHAR, ATTR_PROBE, ATTR_COUNT };
enum { EVENT_ADV_ON=0xf100, EVENT_ADV_OFF, EVENT_PING };
typedef struct { uint16_t id,length; union { uint32_t align; uint8_t bytes[64]; } data; } event_t;
static rt_mq_t events;
static volatile bool queue_fault;
static bool started, service_ready, fault_latched;
static uint8_t active_connection=0xff;
/* Callbacks own this byte; worker reads it only to disconnect after a queue fault. */
static volatile uint8_t callback_connection=0xff;
static uint8_t probe_value[5]={'R','E','A','D','Y'};
static uint16_t probe_length=5;
static uint8_t service_uuid[16]=UUID(0);
static uint8_t info[]="ShaheCat " PET_FIRMWARE_VERSION " BLE probe; RTC sync not enabled";
static sibles_hdl service;
SIBLES_ADVERTISING_CONTEXT_DECLAR(advertising);

BLE_GATT_SERVICE_DEFINE_128(attributes) {
    BLE_GATT_SERVICE_DECLARE(ATTR_SERVICE,SERIAL_UUID_16_PRI_SERVICE,BLE_GATT_PERM_READ_ENABLE),
    BLE_GATT_CHAR_DECLARE(ATTR_INFO_CHAR,SERIAL_UUID_16_CHARACTERISTIC,BLE_GATT_PERM_READ_ENABLE),
    BLE_GATT_CHAR_VALUE_DECLARE(ATTR_INFO,UUID(1),BLE_GATT_PERM_READ_ENABLE,
        BLE_GATT_VALUE_PERM_UUID_128|BLE_GATT_VALUE_PERM_RI_ENABLE,sizeof(info)-1),
    BLE_GATT_CHAR_DECLARE(ATTR_PROBE_CHAR,SERIAL_UUID_16_CHARACTERISTIC,BLE_GATT_PERM_READ_ENABLE),
    BLE_GATT_CHAR_VALUE_DECLARE(ATTR_PROBE,UUID(2),BLE_GATT_PERM_READ_ENABLE|BLE_GATT_PERM_WRITE_REQ_ENABLE,
        BLE_GATT_VALUE_PERM_UUID_128|BLE_GATT_VALUE_PERM_RI_ENABLE,5),
};

static void reset_probe(void) { memcpy(probe_value,"READY",5); probe_length=5; }
static bool enqueue(uint16_t id,const void *data,unsigned length)
{
    if (!events || length>sizeof(((event_t *)0)->data.bytes) || (length && !data)) return false;
    event_t event={0}; event.id=id; event.length=(uint16_t)length;
    if (length) memcpy(event.data.bytes,data,length);
    if (rt_mq_send(events,&event,sizeof(event))!=RT_EOK) { queue_fault=true; return false; }
    return true;
}
static uint8_t *read_value(uint8_t connection,uint8_t attribute,uint16_t *length)
{
    *length=0;
    if (queue_fault || callback_connection==0xff || connection!=callback_connection) return NULL;
    if (attribute==ATTR_INFO) { *length=sizeof(info)-1; return info; }
    if (attribute==ATTR_PROBE) { *length=probe_length; return probe_value; }
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
    BLE_GATT_SERVICE_INIT_128(definition,attributes,ATTR_COUNT,
        BLE_GATT_SERVICE_PERM_NOAUTH|BLE_GATT_SERVICE_PERM_UUID_128,service_uuid);
    service=sibles_register_svc_128(&definition);
    if (!service) { rt_kprintf("[pet:BLE] GATT registration failed\n"); return false; }
    sibles_register_cbk(service,read_value,write_value);
    char name[24]="ShaheCat";
    bd_addr_t address;
    if (!ble_get_public_address(&address))
        rt_snprintf(name,sizeof(name),"ShaheCat-%02X%02X%02X",address.addr[2],address.addr[1],address.addr[0]);
    unsigned length=strlen(name);
    ble_gap_dev_name_t *device_name=rt_malloc(sizeof(*device_name)+length);
    sibles_adv_type_name_t *scan_name=rt_malloc(sizeof(*scan_name)+length);
    sibles_adv_type_srv_uuid_t *uuid=rt_malloc(sizeof(*uuid)+sizeof(sibles_adv_uuid_t));
    if (!device_name || !scan_name || !uuid) {
        rt_free(device_name); rt_free(scan_name); rt_free(uuid);
        rt_kprintf("[pet:BLE] advertising allocation failed\n"); return false;
    }
    device_name->len=length; memcpy(device_name->name,name,length);
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
    if (!result) result=sibles_advertising_init(advertising,&parameters);
    rt_free(device_name); rt_free(scan_name); rt_free(uuid);
    if (!result) result=sibles_advertising_start(advertising);
    if (result) { rt_kprintf("[pet:BLE] advertising setup failed=%u\n",result); return false; }
    rt_kprintf("[pet:BLE] name=%s; open LightBlue, scan and connect\n",name);
    return true;
}
static void handle(const event_t *event)
{
    if (event->id==BLE_POWER_ON_IND) {
        if (!service_ready) service_ready=register_service();
    } else if (event->id==CONNECTION_MANAGER_CONNCTED_IND) {
        const connection_manager_connect_ind_t *p=(const void *)event->data.bytes;
        if (active_connection!=0xff || !service_ready) { disconnect(p->conn_idx); return; }
        active_connection=p->conn_idx;
        rt_kprintf("[pet:BLE] connected index=%u; link only, read/write not verified\n",p->conn_idx);
    } else if (event->id==BLE_GAP_DISCONNECTED_IND) {
        const ble_gap_disconnected_ind_t *p=(const void *)event->data.bytes;
        if (active_connection==p->conn_idx) active_connection=0xff;
        rt_kprintf("[pet:BLE] disconnected reason=%u; SDK restarts advertising\n",p->reason);
    } else if (event->id==EVENT_ADV_ON) {
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
    sifli_ble_enable();
    rt_tick_t startup=rt_tick_get();
    bool timeout_reported=false;
    for (;;) {
        event_t event;
        rt_err_t received=rt_mq_recv(events,&event,sizeof(event),rt_tick_from_millisecond(100));
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
    if (!pet_ble_probe_selftest()) { rt_kprintf("[pet:BLE-TEST] protocol FAIL\n"); return -RT_ERROR; }
    rt_kprintf("[pet:BLE-TEST] protocol PASS (RAM only, no radio verdict)\n");
    events=rt_mq_create("pet_ble",sizeof(event_t),16,RT_IPC_FLAG_FIFO);
    if (!events) return -RT_ENOMEM;
    rt_thread_t thread=rt_thread_create("pet_ble",task,NULL,4096,21,10);
    if (!thread) { rt_mq_delete(events); events=NULL; return -RT_ENOMEM; }
    rt_err_t result=rt_thread_startup(thread);
    if (result!=RT_EOK) { rt_thread_delete(thread); rt_mq_delete(events); events=NULL; return result; }
    started=true;
    return RT_EOK;
}
