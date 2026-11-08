#ifndef PET_LINK_H
#define PET_LINK_H
#include <stdint.h>
/* Copied values only: UI is independent of SDK callbacks and RT-Thread. */
typedef enum { PET_LINK_STARTING, PET_LINK_WAITING, PET_LINK_CONNECTED, PET_LINK_ERROR } pet_link_state_t;
typedef struct { pet_link_state_t state; char name[24]; } pet_link_t;
typedef struct { uint8_t food, mood, scene, phase, volume, sound_mode; } pet_phone_status_t;
#endif
