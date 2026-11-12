/* PAN/HID connection sequence based on the verified peripheral_lab PAN service. */
#include "pet_voice_pan.h"
#include "rtthread.h"
#include "bf0_ble_common.h"
#include "bts2_app_inc.h"
#include "bt_connection_manager.h"
#include "lwip/tcpip.h"
#include "lwip/netif.h"
#include "lwip/dhcp.h"
#include <stdatomic.h>
#include <string.h>

typedef struct {
    uint16_t type, id, size;
    uint8_t data[32];
} bt_event_t;
static rt_mq_t events;
static bool ready, active, acl, paired, pan, pairing;
static uint8_t peer[6];
static uint32_t pair_code;
static bool pair_in_flight, discovery_pending;
static rt_tick_t pair_deadline, connect_at, discover_at;
static atomic_bool online, network_enabled, network_pending;
static struct netif *old_route, *pan_route; /* TCP/IP thread only. */
static rt_tick_t next_network_check;

static rt_tick_t after(unsigned ms)
{
    return rt_tick_get() + rt_tick_from_millisecond(ms);
}

static bool due(rt_tick_t when)
{
    return (int32_t)(rt_tick_get() - when) >= 0;
}

static void schedule_discovery(unsigned delay_ms)
{
    discovery_pending = active;
    discover_at = after(delay_ms);
}

static void recover_pairing(const char *reason)
{
    rt_kprintf("[pet:VOICE] pair recovery: %s\n", reason);
    if (pairing) bt_interface_user_confirm_res(peer, 0);
    if (acl || pair_in_flight) bt_interface_disconnect_req(peer);
    acl = paired = pan = pairing = pair_in_flight = false;
    atomic_store(&network_enabled, false);
    atomic_store(&online, false);
    schedule_discovery(600);
}

static int queue_event(uint16_t type, uint16_t id, uint8_t *data, uint16_t size)
{
    if (type != BT_NOTIFY_COMMON && type != BT_NOTIFY_PAN && type != BT_NOTIFY_HID) return 0;
    bt_event_t e = {.type = type, .id = id, .size = size};
    if (size > sizeof(e.data) || (size && !data)) return 0;
    if (size) memcpy(e.data, data, size);
    if (rt_mq_send(events, &e, sizeof(e)) != RT_EOK)
        rt_kprintf("[pet:VOICE] PAN event queue full\n");
    return 0;
}

static void network_check(void *unused)
{
    (void)unused;
    bool up = false;
    if (atomic_load(&network_enabled))
    {
        for (struct netif *n = netif_list; n; n = n->next)
        {
            if (n->name[0] != 'b' || n->name[1] < '0' || n->name[1] > '9' || !netif_is_link_up(n))
                continue;
            if (n != pan_route)
            {
                if (!pan_route) old_route = netif_default;
                pan_route = n;
                netif_set_default(n);
                if (!dhcp_supplied_address(n)) dhcp_start(n);
            }
            up = netif_is_up(n) && !ip4_addr_isany_val(*netif_ip4_addr(n));
            break;
        }
    }
    if (!up && !atomic_load(&network_enabled) && pan_route)
    {
        struct netif *n = netif_list;
        while (n && n != old_route) n = n->next;
        if (netif_default == pan_route) netif_set_default(n);
        pan_route = old_route = NULL;
    }
    atomic_store(&online, up && atomic_load(&network_enabled));
    atomic_store(&network_pending, false);
}

static void process(const bt_event_t *e)
{
    if (e->type == BT_NOTIFY_COMMON)
    {
        switch (e->id)
        {
        case BT_NOTIFY_COMMON_BT_STACK_READY:
            ready = true;
            rt_kprintf("[pet:VOICE] classic Bluetooth ready\n");
            break;
        case BT_NOTIFY_COMMON_SCAN_ENB_CFM_IND:
            if (e->size == 1)
            {
                rt_kprintf("[pet:VOICE] scan mode=%u\n", (unsigned)e->data[0]);
                if (e->data[0] == 3) discovery_pending = false;
                else if (active && !acl && !pair_in_flight && !paired) schedule_discovery(800);
            }
            break;
        case BT_NOTIFY_COMMON_IO_CAPABILITY_IND:
            if (e->size >= sizeof(peer))
                bt_interface_io_req_res((uint8_t *)e->data,
                    active && (!(acl || paired || pairing) || !memcmp(peer, e->data, sizeof(peer)))
                        ? IO_CAPABILITY_DISPLAY_YES_NO : IO_CAPABILITY_REJECT_REQ, 1, 1);
            break;
        case BT_NOTIFY_COMMON_USER_CONFIRM_IND:
            if (e->size >= sizeof(bt_notify_pair_confirm_t))
            {
                bt_notify_pair_confirm_t request;
                memcpy(&request, e->data, sizeof(request));
                if (!active || ((acl || paired || pairing) && memcmp(peer, request.mac.addr, sizeof(peer))))
                {
                    bt_interface_user_confirm_res(request.mac.addr, 0);
                    break;
                }
                memcpy(peer, request.mac.addr, sizeof(peer));
                pair_code = request.num_val;
                pair_deadline = after(25000);
                pairing = pair_in_flight = true;
                discovery_pending = false;
            }
            break;
        case BT_NOTIFY_COMMON_PAIR_IND:
            if (e->size >= sizeof(bt_notify_device_base_info_t))
            {
                bt_notify_device_base_info_t event;
                memcpy(&event, e->data, sizeof(event));
                rt_kprintf("[pet:VOICE] classic pair result=0x%02x\n", (unsigned)event.res);
                if (!active || !(acl || pair_in_flight) || memcmp(peer, event.mac.addr, sizeof(peer))) break;
                if (event.res) recover_pairing("phone or stack rejected pairing");
                else
                {
                    pairing = pair_in_flight = false;
                    paired = true;
                    connect_at = after(3000);
                }
            }
            break;
        case BT_NOTIFY_COMMON_ACL_CONNECTED:
            if (e->size >= sizeof(bt_notify_device_acl_conn_info_t))
            {
                bt_notify_device_acl_conn_info_t event;
                memcpy(&event, e->data, sizeof(event));
                if (event.res)
                {
                    if (active && !acl && !pair_in_flight) schedule_discovery(600);
                    break;
                }
                if (!active || ((acl || pairing) && memcmp(peer, event.mac.addr, sizeof(peer))))
                {
                    bt_interface_disconnect_req(event.mac.addr);
                    break;
                }
                memcpy(peer, event.mac.addr, sizeof(peer));
                acl = true;
                discovery_pending = false;
            }
            break;
        case BT_NOTIFY_COMMON_ENCRYPTION:
            if (active && e->size >= sizeof(peer) && (!paired || !memcmp(peer, e->data, sizeof(peer))))
            {
                memcpy(peer, e->data, sizeof(peer));
                paired = true;
                pairing = pair_in_flight = false;
                connect_at = after(3000);
            }
            break;
        case BT_NOTIFY_COMMON_ACL_DISCONNECTED:
            if (e->size >= sizeof(peer) && !memcmp(peer, e->data, sizeof(peer)))
            {
                acl = paired = pan = pairing = pair_in_flight = false;
                atomic_store(&network_enabled, false);
                atomic_store(&online, false);
                schedule_discovery(600);
                rt_kprintf("[pet:VOICE] classic link disconnected\n");
            }
            break;
        default:
            break;
        }
        return;
    }
    if (e->size < sizeof(bt_notify_profile_state_info_t)) return;
    bt_notify_profile_state_info_t event;
    memcpy(&event, e->data, sizeof(event));
    if (e->type == BT_NOTIFY_HID && e->id == BT_NOTIFY_HID_PROFILE_CONNECTED && active && !event.res)
    {
        if (paired && memcmp(peer, event.mac.addr, sizeof(peer))) return;
        memcpy(peer, event.mac.addr, sizeof(peer));
        paired = true;
        pairing = pair_in_flight = false;
        connect_at = after(1000);
    }
    if (e->type != BT_NOTIFY_PAN) return;
    if (e->id == BT_NOTIFY_PAN_PROFILE_CONNECTED && !event.res)
    {
        if (!active || (paired && memcmp(peer, event.mac.addr, sizeof(peer))))
        {
            bt_interface_disc_ext(event.mac.addr, BT_PROFILE_PAN);
            return;
        }
        memcpy(peer, event.mac.addr, sizeof(peer));
        pan = paired = true;
        bt_interface_set_scan_mode(0, 0);
        atomic_store(&network_enabled, true);
    }
    if (e->id == BT_NOTIFY_PAN_PROFILE_DISCONNECTED && !memcmp(peer, event.mac.addr, sizeof(peer)))
    {
        pan = false;
        connect_at = after(2000);
        atomic_store(&network_enabled, false);
        atomic_store(&online, false);
    }
}

bool pet_voice_pan_init(void)
{
    events = rt_mq_create("vc_bt", sizeof(bt_event_t), 24, RT_IPC_FLAG_FIFO);
    if (!events) return false;
    bt_cm_set_profile_target(BT_CM_HID, BT_LINK_PHONE, 0);
    if (bt_interface_register_bt_event_notify_callback(queue_event) == BT_INTERFACE_STATUS_OK) return true;
    rt_mq_delete(events);
    events = NULL;
    return false;
}

void pet_voice_pan_poll(bool enabled, bool accept)
{
    bt_event_t e;
    while (rt_mq_recv(events, &e, sizeof(e), 0) == RT_EOK) process(&e);
    if (ready && active != enabled)
    {
        active = enabled;
        if (active)
        {
            char name[] = "ShaheCat Voice";
            bt_interface_set_local_name(strlen(name), name);
            schedule_discovery(0);
            connect_at = after(100);
        }
        else
        {
            bt_interface_set_scan_mode(0, 0);
            if (pairing) bt_interface_user_confirm_res(peer, 0);
            discovery_pending = false;
            if (acl || paired || pair_in_flight)
            {
                bt_interface_cancel_connect_req(peer);
                bt_interface_disc_ext(peer, BT_PROFILE_PAN);
                bt_interface_disc_ext(peer, BT_PROFILE_HID);
                bt_interface_disconnect_req(peer);
            }
            acl = paired = pan = pairing = pair_in_flight = false;
            atomic_store(&network_enabled, false);
            atomic_store(&online, false);
        }
    }
    if (pairing && accept)
    {
        bt_interface_user_confirm_res(peer, 1);
        pairing = false;
        pair_deadline = after(20000);
    }
    if (pair_in_flight && due(pair_deadline)) recover_pairing("confirmation timeout");
    if (active && discovery_pending && !acl && !pair_in_flight && !paired && due(discover_at))
    {
        unsigned result = bt_interface_set_scan_mode(1, 1);
        rt_kprintf("[pet:VOICE] request discovery result=%u\n", result);
        discover_at = after(1500); /* Wait for asynchronous scan confirmation. */
    }
    if (active && paired && !pan && !pairing && due(connect_at))
    {
        bt_interface_conn_ext(peer, BT_PROFILE_PAN);
        connect_at = after(15000);
    }
    if (due(next_network_check) && !atomic_exchange(&network_pending, true))
    {
        next_network_check = after(250);
        if (tcpip_try_callback(network_check, NULL) != ERR_OK) atomic_store(&network_pending, false);
    }
}

bool pet_voice_pan_online(void)
{
    return atomic_load(&online);
}

bool pet_voice_pan_pairing(uint32_t *code)
{
    if (pairing) *code = pair_code;
    return pairing;
}
