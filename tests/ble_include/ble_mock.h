#ifndef BLE_MOCK_H
#define BLE_MOCK_H
#include <stdint.h>
#define BLE_POWER_ON_IND 1
#define CONNECTION_MANAGER_CONNCTED_IND 2
#define BLE_GAP_DISCONNECTED_IND 3
#define SIBLES_ADV_EVT_ADV_STARTED 4
#define SIBLES_ADV_EVT_ADV_STOPPED 5
#define SIBLES_SEARCH_SVC_RSP 6
#define SIBLES_REGISTER_REMOTE_SVC_RSP 7
#define SIBLES_READ_REMOTE_VALUE_RSP 8
#define BLE_GAP_ENCRYPT_IND 9
#define BLE_GAP_BOND_IND 10
#define SIBLES_READ_REMOTE_VALUE_ERROR_IND 11
#define SIBLES_REMOTE_SVC_CHANGE_IND 12
#define BLE_GAP_SECURITY_REQUEST_CNF 13
#define LE_SECURITY_LEVEL_SEC_CON_BOND 5
#define CM_STATUS_OK 0
#define GAPC_PAIRING_SUCCEED 1
#define GAPC_PAIRING_FAILED 2
#define SIBLES_ERROR_REMOTE_HANDLE 0xffff
#define SIBLES_READ 1
#define CO_ERROR_REMOTE_USER_TERM_CON 19
#define GAPM_STATIC_ADDR 0
#define SIBLES_ADV_CONNECT_MODE 1
#define BLE_GATT_SERVICE_PERM_NOAUTH 0
#define BLE_GATT_SERVICE_PERM_UUID_128 1
#define SIBLES_ADVERTISING_CONTEXT_DECLAR(name) static void *name=(void *)1
typedef struct { uint8_t uuid[16]; int permission,value_permission,length; } mock_attribute_t;
#define BLE_GATT_SERVICE_DEFINE_128(name) static mock_attribute_t name[] =
#define BLE_GATT_SERVICE_DECLARE(idx,uuid,perm) [idx]={uuid,perm,0,0}
#define BLE_GATT_CHAR_DECLARE(idx,uuid,perm) [idx]={uuid,perm,0,0}
#define BLE_GATT_CHAR_VALUE_DECLARE(idx,uuid,perm,vperm,len) [idx]={uuid,perm,vperm,len}
#define BLE_GATT_PERM_READ_ENABLE 1
#define BLE_GATT_PERM_WRITE_REQ_ENABLE 2
#define BLE_GATT_VALUE_PERM_UUID_128 4
#define BLE_GATT_VALUE_PERM_RI_ENABLE 8
#define SERIAL_UUID_16_PRI_SERVICE SERIAL_UUID_16(0x2800)
#define SERIAL_UUID_16_CHARACTERISTIC SERIAL_UUID_16(0x2803)
#define BLE_GATT_SERVICE_INIT_128(name,attrs,count,perms,uuid) int name=0; (void)attrs; (void)uuid
#define BLE_EVENT_REGISTER(callback,context)
typedef void *sibles_hdl;
typedef struct { uint8_t addr[6]; } bd_addr_t;
typedef struct { uint8_t conn_idx; } connection_manager_connect_ind_t;
typedef struct { uint8_t conn_idx,reason; } ble_gap_disconnected_ind_t;
typedef ble_gap_disconnected_ind_t ble_gap_disconnect_t;
typedef struct { uint8_t idx; uint16_t len,offset; uint8_t *value; } sibles_set_cbk_t;
typedef struct { uint8_t status; } sibles_adv_evt_startted_t;
typedef struct { uint8_t reason; } sibles_adv_evt_stopped_t;
typedef struct { uint8_t len; uint8_t name[1]; } ble_gap_dev_name_t;
typedef struct { uint8_t name_len; uint8_t name[1]; } sibles_adv_type_name_t;
typedef struct { uint8_t uuid_len; union { uint8_t uuid_128[16]; } uuid; } sibles_adv_uuid_t;
typedef struct { uint8_t count; sibles_adv_uuid_t uuid_list[1]; } sibles_adv_type_srv_uuid_t;
typedef struct {
 int own_addr_type;
 struct { int adv_mode; struct { struct { unsigned duration,interval; } conn_config; } mode_config; int max_tx_pwr,is_auto_restart; } config;
 struct { sibles_adv_type_name_t *completed_name; } rsp_data;
 struct { sibles_adv_type_srv_uuid_t *completed_uuid; } adv_data;
 uint8_t (*evt_handler)(uint8_t,void *,void *);
} sibles_advertising_para_t;
sibles_hdl sibles_register_svc_128(void *);
void sibles_register_cbk(sibles_hdl,uint8_t *(*)(uint8_t,uint8_t,uint16_t *),uint8_t (*)(uint8_t,sibles_set_cbk_t *));
uint8_t ble_get_public_address(bd_addr_t *);
uint8_t ble_gap_set_dev_name(ble_gap_dev_name_t *);
uint8_t ble_gap_disconnect(ble_gap_disconnect_t *);
uint8_t sibles_advertising_init(void *,sibles_advertising_para_t *);
uint8_t sibles_advertising_start(void *);
uint8_t sibles_advertising_stop(void *);
void sifli_ble_enable(void);
typedef struct { uint8_t conn_idx,auth; } ble_gap_encrypt_ind_t;
typedef struct { uint8_t conn_idx,info; } ble_gap_bond_ind_t;
typedef struct { uint16_t attr_hdl;uint8_t uuid_len,uuid[16]; } mock_descriptor_t;
#define sibles_disc_char_desc_ind mock_descriptor
struct mock_descriptor { uint16_t attr_hdl;uint8_t uuid_len,uuid[16]; };
typedef struct { uint16_t attr_hdl,pointer_hdl;uint8_t prop,uuid_len,uuid[16],desc_count; struct mock_descriptor desc[]; } sibles_svc_search_char_t;
typedef struct { uint16_t hdl_start,hdl_end;uint8_t uuid_len,uuid[16],char_count;uint8_t *att_db; } sibles_svc_remote_svc_t;
typedef struct { uint8_t conn_idx,result,search_svc_len,search_uuid[16];sibles_svc_remote_svc_t *svc; } sibles_svc_search_rsp_t;
typedef struct { uint8_t conn_idx,status; } sibles_register_remote_svc_rsp_t;
typedef struct { uint8_t conn_idx;uint16_t handle,offset,length;uint8_t *value; } sibles_read_remote_value_rsp_t;
typedef struct { int read_type;uint16_t handle,offset,length; } sibles_read_remote_value_req_t;
typedef struct { uint8_t conn_idx,status; } sibles_error_cmp_event_ind_t;
typedef struct { uint8_t conn_idx,status; } ble_gap_security_request_cnf_t;
typedef struct { uint8_t conn_idx;uint16_t start_handle,end_handle; } sibles_remote_svc_change_ind_t;
typedef int (*sibles_remote_svc_cbk)(uint16_t,uint8_t *,uint16_t);
int8_t sibles_search_service(uint8_t,uint8_t,uint8_t *);
uint16_t sibles_register_remote_svc(uint8_t,uint16_t,uint16_t,sibles_remote_svc_cbk);
void sibles_unregister_remote_svc(uint8_t,uint16_t,uint16_t,sibles_remote_svc_cbk);
int8_t sibles_read_remote_value(uint16_t,uint8_t,sibles_read_remote_value_req_t *);
void connection_manager_create_bond(uint8_t);
uint8_t connection_manager_set_link_security(uint8_t,uint8_t);
#endif
