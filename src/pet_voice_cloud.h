#ifndef PET_VOICE_CLOUD_H
#define PET_VOICE_CLOUD_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define PET_VOICE_FRAME_MAX 4096

/* A session belongs to the voice worker. Returned frames last until the next read. */
typedef enum {
    VC_ERROR, VC_ACTIVATION, VC_CONNECTED
} vc_open_result_t;
vc_open_result_t vc_open(char activation[16], bool check_activation, bool (*active)(void));
bool vc_send(uint8_t opcode, const void *data, size_t size);
int vc_read(uint8_t *opcode, const uint8_t **data, size_t *size);
void vc_close(void);

#endif
