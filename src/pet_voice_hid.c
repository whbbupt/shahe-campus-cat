/* PAN compatibility HID: keep control replies and suppress active input. */
#include "bts2_app_inc.h"
#include <string.h>

uint32_t bt_get_class_of_device(void)
{
    return BT_SRVCLS_NETWORK | BT_DEVCLS_LAP | BT_LAP_FULLY;
}

void __real_hid_send_report_req_ext(U16 tid, BTS2S_BD_ADDR *bd, U16 len, U8 *data, BOOL interrupt);

void __wrap_hid_send_report_req_ext(U16 tid, BTS2S_BD_ADDR *bd, U16 len, U8 *data, BOOL interrupt)
{
    if (interrupt) return;
    if (data && len >= 2 && data[0] == 0xa1)
    {
        U8 neutral[256];
        if (len > sizeof(neutral)) return;
        memcpy(neutral, data, 2);
        memset(neutral + 2, 0, len - 2);
        __real_hid_send_report_req_ext(tid, bd, len, neutral, interrupt);
    }
    else __real_hid_send_report_req_ext(tid, bd, len, data, interrupt);
}


/* BT_FINSH is selected for PAN. SDK audio references these only as compatibility
 * hooks when the HFP profiles are absent, as in peripheral_lab. */
#if defined(BT_FINSH) && !defined(AUDIO_BT_AUDIO) && !defined(CFG_HFP_HF) && !defined(CFG_HFP_AG)
void bt_voice_downlink_process(uint8_t ready) { (void)ready; }
void bt_voice_uplink_send(void) { }
#endif
