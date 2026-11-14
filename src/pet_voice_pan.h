#ifndef PET_VOICE_PAN_H
#define PET_VOICE_PAN_H
#include <stdbool.h>
#include <stdint.h>

bool pet_voice_pan_init(void);
void pet_voice_pan_poll(bool enabled, bool accept);
bool pet_voice_pan_online(void);
bool pet_voice_pan_pairing(uint32_t *code);

#endif
