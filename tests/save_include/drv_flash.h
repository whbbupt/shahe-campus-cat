#include <stdint.h>
int rt_flash_read(uint32_t, uint8_t *, int);
int rt_flash_write(uint32_t, const uint8_t *, int);
int rt_flash_erase(uint32_t, int);
int rt_flash_get_total_size(uint32_t);
int rt_flash_get_erase_alignment(uint32_t);
