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
    setvbuf(stdout, NULL, _IONBF, 0);
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
    flash[4] = 99;
    put(flash + 56, crc(flash, 56));
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
    memset(flash, 255, sizeof(flash));
    pw_init(&w);
    ps_open(&s, io, &w);
    assert(ps_save(&s, &w) == PS_OK);
    flash[4] = 1;
    flash[15] &= 1;
    memset(flash + 32, 0, 24);
    put(flash + 56, crc(flash, 56));
    assert(ps_open(&s, io, &w) == PS_OK && w.scene == PW_DORM && w.food == 70);
    w.scene = PW_CAMPUS;
    w.node = 0;
    w.x = 560;
    w.y = 560;
    assert(ps_save(&s, &w) == PS_OK && s.saved[4] == PS_SCHEMA);
    uint8_t *campus = flash + PS_SECTOR_SIZE;
    campus[4] = 2;
    campus[15] &= 1;
    memset(campus + 32, 0, 24);
    put(campus + 56, crc(campus, 56));
    assert(ps_open(&s, io, &w) == PS_OK && w.scene == PW_CAMPUS && w.x == pw_node(PW_CAMPUS, 0).x &&
           w.y == pw_node(PW_CAMPUS, 0).y && w.phase == PW_IDLE && w.order == PW_ORDER_NONE);
    FILE *fixture = fopen("campus-v2-nor.bin", "wb");
    assert(fixture);
    assert(fwrite(flash, sizeof(flash), 1, fixture) == 1);
    fclose(fixture);
    w.food = 69;
    assert(ps_save(&s, &w) == PS_OK && s.saved[4] == PS_SCHEMA);
    assert(ps_open(&s, io, &w) == PS_OK && w.journal_page == 1);
    uint8_t *latest = flash + PS_RECORD_SIZE;
    latest[4] = 3;
    latest[15] &= 1;
    memset(latest + 32, 0, 24);
    put(latest + 28, 12001);
    put(latest + 32, 1200);
    put(latest + 36, 2300);
    put(latest + 40, 14000);
    put(latest + 44, 7000);
    put(latest + 48, 9000);
    put(latest + 52, 50000);
    put(latest + 56, crc(latest, 56));
    assert(ps_open(&s, io, &w) == PS_OK && w.order == PW_ORDER_DELIVERING && w.order_due == 12000);
    assert(w.journal_page == 1 && !w.journal_progress && !w.journal_stamps);
    assert(w.cooldown[PW_EAT] == 1200 && w.cooldown[PW_TOUCH] == 2300 && w.cooldown[PW_WORKOUT] == 14000);
    assert(w.cooldown[PW_STUDY] == 7000 && w.cooldown[PW_CODE] == 9000 && w.cooldown[PW_TAKEOUT] == 50000);
    w.food = 68;
    assert(ps_save(&s, &w) == PS_OK && s.saved[4] == PS_SCHEMA);
    assert(ps_open(&s, io, &w) == PS_OK && w.cooldown[PW_STUDY] == 7000 && w.cooldown[PW_TAKEOUT] == 50000);
    /* Compact v4 retains journal and cooldowns and defaults to music. */
    memset(flash, 255, sizeof(flash));
    w.journal_page = 7; w.journal_stamps = 6; w.journal_progress = 3;
    assert(ps_open(&s, io, &w) == PS_EMPTY);
    assert(ps_save(&s, &w) == PS_OK);
    flash[4] = 4; flash[15] &= 1;
    put(flash + 46, 7); flash[50] = 6; flash[51] = 0; flash[54] = 3; flash[55] = 0;
    put(flash + 56, crc(flash, 56));
    assert(ps_open(&s, io, &w) == PS_OK && w.sound_mode == 2 && w.journal_page == 7);
    assert(w.journal_stamps == 6 && w.journal_progress == 3 && w.cooldown[PW_TAKEOUT] == 50000);
    w.sound_mode = 0;
    assert(ps_save(&s, &w) == PS_OK && s.saved[4] == PS_SCHEMA);
    fixture = fopen("campus-v11-nor.bin", "wb");
    assert(fixture && fwrite(flash, sizeof(flash), 1, fixture) == 1);
    fclose(fixture);
    /* Legacy v5 keeps mute via zero volume; other modes default to five. */
    uint8_t old[64]; memcpy(old, s.saved, 64);
    put(old + 46, 1); memset(old + 50, 0, 6);
    for (unsigned mode=0; mode<3; mode++) {
        memset(flash, 255, sizeof(flash));
        memcpy(flash, old, 64); flash[4]=5; flash[15]=(uint8_t)(mode<<1);
        put(flash+56, crc(flash, 56));
        assert(ps_open(&s, io, &w)==PS_OK && w.sound_mode==mode && w.volume==(mode?5:0));
        assert(ps_save(&s, &w)==PS_OK && s.saved[4]==PS_SCHEMA);
        assert(ps_open(&s, io, &w)==PS_OK && w.volume==(mode?5:0));
    }
    puts("[host:save] legacy v5 sound migration and v6 volume PASS");
    puts("[host:save] schema-v1-v2-v3-v4-to-v8-preserves-life-and-initializes-wallet PASS");
    /* A schema-10 save has no album bits; reading it must not invent unlocks. */
    memset(flash, 255, sizeof(flash));
    pw_init(&w);
    assert(ps_open(&s, io, &w) == PS_EMPTY && ps_save(&s, &w) == PS_OK);
    memcpy(flash, s.saved, 64);
    flash[4] = 10; flash[5] = flash[7] = 0;
    put(flash + 56, crc(flash, 56));
    assert(ps_open(&s, io, &w) == PS_OK && w.dishes == 0);
    w.food = 61;
    assert(ps_save(&s, &w) == PS_OK && s.saved[4] == PS_SCHEMA && s.saved[5] == 0 && s.saved[7] == 0);
    w.dishes = 0x0ABC;
    assert(ps_save(&s, &w) == PS_OK && s.saved[5] == 0xBC && s.saved[7] == 0x0A);
    assert(ps_open(&s, io, &w) == PS_OK && w.dishes == 0x0ABC);
    puts("[host:save] schema10 album-empty migration and current album round-trip PASS");
    memset(flash, 255, sizeof(flash));
    assert(ps_open(&s, io, &w) == PS_EMPTY);
    w.scene = PW_CLASSROOM;
    w.dishes = 0x0ABC;
    assert(ps_save(&s, &w) == PS_OK);
    memcpy(flash, s.saved, 64);
    flash[4] = 11;
    put(flash + 56, crc(flash, 56));
    assert(ps_open(&s, io, &w) == PS_OK && !pw_in_library(&w) && w.dishes == 0x0ABC);
    w.building_id = PW_LIBRARY_BUILDING;
    assert(!ps_visible_saved(&s, &w));
    assert(ps_save(&s, &w) == PS_OK && ps_visible_saved(&s, &w));
    assert(ps_open(&s, io, &w) == PS_OK && pw_in_library(&w) && w.dishes == 0x0ABC);
    assert(w.return_node == PW_LIBRARY_DOOR);
    assert(pw_request(&w, PW_EXIT) == PW_OK && pw_in_library(&w));
    pw_advance(&w, 5000);
    assert(w.scene == PW_CAMPUS && w.node == PW_LIBRARY_DOOR && !pw_in_library(&w));
    puts("[host:save] schema11 compatibility and library restart/exit PASS");
    memset(flash,255,sizeof(flash));pw_init(&w);assert(ps_open(&s,io,&w)==PS_EMPTY);
    w.clock.trusted=true;w.clock.valid=true;w.clock.utc=1704038400U;
    assert(ps_save(&s,&w)==PS_OK && (s.saved[15]&128));
    assert(ps_open(&s,io,&w)==PS_OK && w.clock.trusted && !w.clock.valid && !w.clock.utc);
    w.clock.utc=1704038460U;w.clock.valid=true;w.now=60000;
    assert(ps_poll(&s,&w)==PS_UNCHANGED && ps_visible_saved(&s,&w));
    flash[4]=6;flash[15]&=127;put(flash+56,crc(flash,56));
    assert(ps_open(&s,io,&w)==PS_OK && !w.clock.trusted && w.volume==5);
    puts("[host:save] RTC trust flag, unchanged clock not dirty, and v6 migration PASS");
    puts("[host:save] all PASS (RAM NOR simulation; not physical flash)"); return 0;
}



