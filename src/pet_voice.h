#ifndef PET_VOICE_H
#define PET_VOICE_H
#include <stdbool.h>

#define PET_VOICE_BT_NAME "ShaheCat Talk"

typedef enum {
    PV_UNAVAILABLE, PV_NETWORK, PV_PAIR, PV_CONNECTING, PV_ACTIVATE,
    PV_READY, PV_RECORDING, PV_WAITING, PV_PLAYING, PV_ERROR
} pet_voice_state_t;
typedef enum { PV_VOLUME_LOW, PV_VOLUME_MEDIUM, PV_VOLUME_HIGH, PV_VOLUME_COUNT } pet_voice_volume_t;
typedef struct {
    pet_voice_state_t state;
    char code[16];
    unsigned level;
} pet_voice_view_t;

void pet_voice_start(void);
void pet_voice_page(bool open);
void pet_voice_hold(bool down);
void pet_voice_cancel(void);
void pet_voice_retry(void);
void pet_voice_pair(void);
pet_voice_volume_t pet_voice_cycle_volume(void);
pet_voice_view_t pet_voice_view(void);

#endif
