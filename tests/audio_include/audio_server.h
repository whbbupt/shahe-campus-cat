#ifndef HOST_AUDIO_SERVER_H
#define HOST_AUDIO_SERVER_H
#include <stdint.h>
typedef void *audio_client_t;
typedef struct {
    uint32_t write_samplerate, write_cache_size;
    uint8_t write_channnel_num, write_bits_per_sample;
} audio_parameter_t;
enum { AUDIO_TYPE_LOCAL_MUSIC, AUDIO_TX, AUDIO_DEVICE_SPEAKER };
uint8_t audio_server_get_private_volume(int);
int audio_server_set_private_volume(int, uint8_t);
audio_client_t audio_open2(int, int, audio_parameter_t *, void *, void *, int);
int audio_write(audio_client_t, uint8_t *, uint32_t);
int audio_close(audio_client_t);
#endif
