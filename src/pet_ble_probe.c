#include "pet_ble_probe.h"
#include <string.h>
bool pet_ble_probe_reply(const uint8_t *input, size_t length, uint8_t reply[4])
{
    if (!input || !reply || length != 4 || memcmp(input, "PING", 4)) return false;
    memcpy(reply, "PONG", 4);
    return true;
}
bool pet_ble_probe_selftest(void)
{
    uint8_t reply[4] = {1,2,3,4};
    if (!pet_ble_probe_reply((const uint8_t *)"PING",4,reply) || memcmp(reply,"PONG",4)) return false;
    if (pet_ble_probe_reply(NULL,4,reply) || pet_ble_probe_reply((const uint8_t *)"PING",0,reply) ||
        pet_ble_probe_reply((const uint8_t *)"PING",3,reply) || pet_ble_probe_reply((const uint8_t *)"PING!",5,reply) ||
        pet_ble_probe_reply((const uint8_t *)"ping",4,reply) || pet_ble_probe_reply((const uint8_t *)"PIN\0",4,reply) ||
        pet_ble_probe_reply((const uint8_t *)"PING",4,NULL)) return false;
    return !memcmp(reply,"PONG",4);
}
