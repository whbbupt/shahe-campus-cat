#include "pet_ble_probe.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void)
{
    assert(pet_ble_probe_selftest());
    uint8_t guarded[6]={0xa5,1,2,3,4,0x5a};
    assert(pet_ble_probe_reply((const uint8_t *)"PING",4,guarded+1));
    assert(!memcmp(guarded+1,"PONG",4) && guarded[0]==0xa5 && guarded[5]==0x5a);
    const size_t lengths[]={0,1,2,3,5,20,(size_t)-1};
    for (unsigned i=0;i<sizeof(lengths)/sizeof(lengths[0]);++i) {
        assert(!pet_ble_probe_reply((const uint8_t *)"PING",lengths[i],guarded+1));
        assert(!memcmp(guarded+1,"PONG",4));
    }
    for (unsigned i=0;i<4;++i) {
        uint8_t changed[4]={'P','I','N','G'}; changed[i]=0;
        assert(!pet_ble_probe_reply(changed,4,guarded+1));
        assert(!memcmp(guarded+1,"PONG",4));
    }
    assert(!pet_ble_probe_reply(NULL,4,guarded+1));
    assert(!pet_ble_probe_reply((const uint8_t *)"PING",4,NULL));
    puts("[host:ble-probe] shared selftest, PING, length/null/binary rejection and output guards PASS; no physical BLE verdict");
    return 0;
}
