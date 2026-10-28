#include "pet_save_flash.h"
#include "rtthread.h"
#include "drv_flash.h"
#include "ptab.h"
#if !defined(PET_SAVE_REGION_START_ADDR) || !defined(PET_SAVE_REGION_SIZE)
#error "The board-local partition table must define PET_SAVE_REGION"
#endif
static bool bounds(uint32_t offset, size_t size)
{
    return offset <= PS_AREA_SIZE && size <= PS_AREA_SIZE - offset;
}
static bool read_flash(void *ctx, uint32_t offset, uint8_t *data, size_t size)
{
    (void)ctx;
    return bounds(offset, size) && rt_flash_read(PET_SAVE_REGION_START_ADDR + offset, data, (int)size) == (int)size;
}
static bool write_flash(void *ctx, uint32_t offset, const uint8_t *data, size_t size)
{
    (void)ctx;
    return bounds(offset, size) && rt_flash_write(PET_SAVE_REGION_START_ADDR + offset, data, (int)size) == (int)size;
}
static bool erase_flash(void *ctx, uint32_t offset, size_t size)
{
    (void)ctx;
    return bounds(offset, size) && offset % PS_SECTOR_SIZE == 0 && size == PS_SECTOR_SIZE &&
           rt_flash_erase(PET_SAVE_REGION_START_ADDR + offset, (int)size) == RT_EOK;
}
bool ps_flash_io(ps_io_t *io)
{
    uint32_t base = PET_SAVE_REGION_START_ADDR;
    int total = rt_flash_get_total_size(base), alignment = rt_flash_get_erase_alignment(base);
    if (PET_SAVE_REGION_SIZE != PS_AREA_SIZE || base % PS_SECTOR_SIZE ||
        alignment <= 0 || PS_SECTOR_SIZE % (unsigned)alignment || total <= 0 ||
        base < 0x12000000U || base - 0x12000000U + PS_AREA_SIZE > (uint32_t)total)
    {
        rt_kprintf("[pet:SAVE] unavailable capacity=%d alignment=%d\n", total, alignment);
        return false;
    }
    *io = (ps_io_t){NULL, read_flash, write_flash, erase_flash};
    return true;
}
