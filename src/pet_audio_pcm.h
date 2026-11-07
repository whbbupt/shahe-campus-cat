#ifndef PET_AUDIO_PCM_H
#define PET_AUDIO_PCM_H
#include "pet_audio.h"
#define PA_SAMPLE_RATE 16000U
typedef struct { const int16_t *data; uint32_t count; } pa_pcm_t;
extern const pa_pcm_t pa_music;
extern const pa_pcm_t pa_cues[PA_CUE_COUNT];
#endif
