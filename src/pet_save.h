#ifndef PET_SAVE_H
#define PET_SAVE_H
#include "pet_world.h"
#include <stddef.h>
#define PS_RECORD_SIZE 64U
#define PS_SECTOR_SIZE 4096U
#define PS_AREA_SIZE (2U * PS_SECTOR_SIZE)
typedef enum { PS_OK, PS_EMPTY, PS_WAIT, PS_UNCHANGED, PS_IO_ERROR, PS_INVALID, PS_VERSION } ps_result_t;
typedef struct
{
    void *context;
    bool (*read)(void *, uint32_t, uint8_t *, size_t);
    bool (*write)(void *, uint32_t, const uint8_t *, size_t);
    bool (*erase)(void *, uint32_t, size_t);
} ps_io_t;
typedef struct
{
    ps_io_t io;
    uint8_t saved[PS_RECORD_SIZE], pending_key[5];
    uint32_t sequence;
    int sector;
    bool enabled, has_saved, pending, attempted;
    uint64_t dirty_since, last_attempt, last_success;
} pet_save_t;
ps_result_t ps_open(pet_save_t *s, ps_io_t io, pet_world_t *world);
ps_result_t ps_save(pet_save_t *s, const pet_world_t *world);
/* True only for current visible values with no unfinished business intent; no IO. */
bool ps_visible_saved(const pet_save_t *s, const pet_world_t *world);
ps_result_t ps_poll(pet_save_t *s, const pet_world_t *world);
bool ps_selftest(void (*report)(const char *, bool));
#endif
