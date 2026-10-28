#include "pet_save.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static void report(const char *name, bool ok)
{
    printf("[host:save] %s %s\n", name, ok ? "PASS" : "FAIL");
}
static uint8_t flash[PS_AREA_SIZE];
static int reads, fail_at;
static bool read_io(void *c, uint32_t o, uint8_t *p, size_t n)
{
    (void)c; assert(o + n <= PS_AREA_SIZE);
    if (++reads == fail_at) return false;
    memcpy(p, flash + o, n); return true;
}
static bool write_io(void *c, uint32_t o, const uint8_t *p, size_t n)
{
    (void)c; assert(o + n <= PS_AREA_SIZE);
    for (size_t i = 0; i < n; i++) { assert((flash[o+i] & p[i]) == p[i]); flash[o+i] &= p[i]; }
    return true;
}
static bool erase_io(void *c, uint32_t o, size_t n)
{
    (void)c; assert(o % 4096 == 0 && n == 4096 && o+n<=PS_AREA_SIZE); memset(flash+o,255,n); return true;
}
static uint32_t crc(const uint8_t *p, unsigned n)
{
    uint32_t c=~0U;
    while(n--) { c^=*p++; for(unsigned i=0;i<8;i++) c=(c>>1)^(0xEDB88320U&(0U-(c&1))); }
    return ~c;
}
static void put(uint8_t *p, uint32_t v) { for(unsigned i=0;i<4;i++) p[i]=(uint8_t)(v>>(i*8)); }
int main(void)
{
    assert(ps_selftest(report));
    ps_io_t io={NULL,read_io,write_io,erase_io}; pet_save_t s; pet_world_t w;
    memset(flash,255,sizeof(flash)); pw_init(&w); assert(ps_open(&s,io,&w)==PS_EMPTY);
    assert(ps_save(&s,&w)==PS_OK); w.food=51; reads=0;
    /* 128 scan reads + one free-slot read + body verification + final verification. */
    fail_at=131; assert(ps_save(&s,&w)==PS_IO_ERROR); fail_at=0;
    w.food=49; assert(ps_save(&s,&w)==PS_OK && s.sequence==3);
    assert(ps_open(&s,io,&w)==PS_OK && w.food==49);
    puts("[host:save] committed-but-readback-failed reconciliation PASS");
    /* Check schema refusal with a valid CRC, and never write over unknown format. */
    flash[4]=2; put(flash+56,crc(flash,56));
    assert(ps_open(&s,io,&w)==PS_VERSION && !s.enabled);
    assert(ps_save(&s,&w)==PS_IO_ERROR);
    puts("[host:save] unknown-schema-write-protection PASS");
    memset(flash,255,sizeof(flash)); pw_init(&w); ps_open(&s,io,&w); ps_save(&s,&w);
    put(flash+8,0xFFFFFFFFU); put(flash+56,crc(flash,56));
    assert(ps_open(&s,io,&w)==PS_OK && s.sequence==0xFFFFFFFFU);
    w.food=44; assert(ps_save(&s,&w)==PS_OK && s.sequence==0);
    assert(ps_open(&s,io,&w)==PS_OK && w.food==44);
    puts("[host:save] sequence-wrap PASS");
    /* Out-of-range field with valid CRC must still be rejected. */
    flash[PS_SECTOR_SIZE+12]=101; put(flash+PS_SECTOR_SIZE+56,crc(flash+PS_SECTOR_SIZE,56));
    assert(ps_open(&s,io,&w)==PS_OK && w.food==70);
    puts("[host:save] semantic-range-validation PASS");
    puts("[host:save] all PASS (RAM NOR simulation; not physical flash)"); return 0;
}
