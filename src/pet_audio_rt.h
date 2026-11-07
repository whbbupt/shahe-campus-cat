#ifndef PET_AUDIO_RT_H
#define PET_AUDIO_RT_H
#include "pet_world.h"
void pet_audio_start(const pet_world_t *);
void pet_audio_sync(const pet_world_t *);
bool pet_audio_available(void);
#endif
