/* Fixed little-endian journal. This module never knows a physical flash address. */
#include "pet_save.h"
#include <string.h>
#define MAGIC 0x31544143U
#define COMMIT 0x5A17C0DEU
#define SCHEMA 4U
enum { ORDER_OFFSET = 28, WORKOUT_OFFSET = 40, TAKEOUT_OFFSET = 52 };
static uint32_t get32(const uint8_t *p)
{
    return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}
static void put32(uint8_t *p, uint32_t n)
{
    for (unsigned i = 0; i < 4; i++) p[i] = (uint8_t)(n >> (8 * i));
}
static uint16_t get16(const uint8_t *p)
{
    return (uint16_t)((uint16_t)p[0] | (uint16_t)p[1] << 8);
}
static void put16(uint8_t *p, uint16_t value)
{
    p[0] = (uint8_t)value;
    p[1] = (uint8_t)(value >> 8);
}
static const pw_command_t saved_actions[] = {
    PW_EAT, PW_TOUCH, PW_STUDY, PW_CODE, PW_WORKOUT, PW_TAKEOUT, PW_MEMORY};
static const uint32_t saved_limits[] = {
    2000, 3000, 8000, 10000, PW_WORKOUT_COOLDOWN_MS, PW_TAKEOUT_COOLDOWN_MS, PW_MEMORY_COOLDOWN_MS};

static uint32_t crc(const uint8_t *p, unsigned n)
{
    uint32_t c = ~0U;
    while (n--)
    {
        c ^= *p++;
        for (unsigned i = 0; i < 8; i++) c = (c >> 1) ^ (0xEDB88320U & (0U - (c & 1U)));
    }
    return ~c;
}
static bool newer(uint32_t a, uint32_t b)
{
    return a != b && (uint32_t)(a - b) < 0x80000000U;
}
static bool committed(const uint8_t *r)
{
    return get32(r) == MAGIC && get32(r + 60) == COMMIT && get32(r + 56) == crc(r, 56);
}
static bool known_header(const uint8_t *r)
{
    return r[4] >= 1 && r[4] <= SCHEMA && !r[5] && r[6] == PS_RECORD_SIZE && !r[7];
}
static uint32_t order_code(const uint8_t *r)
{
    return r[4] >= 3 ? get32(r + ORDER_OFFSET) : 0;
}
static pw_order_t order_state(const uint8_t *r)
{
    uint32_t code = order_code(r);
    return code == 0 ? PW_ORDER_NONE : code == 1 ? PW_ORDER_READY : PW_ORDER_DELIVERING;
}
static void visible_key(const uint8_t *r, uint8_t key[15])
{
    memcpy(key, r + 12, 4);
    key[4] = (uint8_t)order_state(r);
    memset(key + 5, 0, 10);
    if (r[4] == SCHEMA)
        memcpy(key + 5, r + 46, 10);
    else
        key[5] = 1; /* Older snapshots start on journal page one. */
}
static bool valid(const uint8_t *r)
{
    if (!committed(r) || !known_header(r) || r[12] > 100 || r[13] > 100 || r[15] > 1 ||
        r[14] > (r[4] == 1 ? PW_CANTEEN : r[4] == 2 ? PW_CAMPUS : PW_GYM) ||
        (r[15] && r[14] != PW_DORM) || get32(r + 16) >= 300000 || get32(r + 20) >= 600000 ||
        get32(r + 24) >= 60000)
        return false;
    if (r[4] == SCHEMA)
    {
        if (!get32(r + 46) || r[54] > 7 || r[55] > 7 || order_code(r) > PW_DELIVERY_MS + 1)
            return false;
        for (unsigned i = 0; i < sizeof(saved_actions) / sizeof(saved_actions[0]); i++)
            if (get16(r + 32 + 2 * i) > saved_limits[i])
                return false;
        return true;
    }
    if (get32(r + 32) > 2000 || get32(r + 36) > 3000 ||
        get32(r + 44) > 8000 || get32(r + 48) > 10000)
        return false;
    if (r[4] < 3)
        return !get32(r + ORDER_OFFSET) && !get32(r + WORKOUT_OFFSET) && !get32(r + TAKEOUT_OFFSET);
    return order_code(r) <= PW_DELIVERY_MS + 1 &&
           get32(r + WORKOUT_OFFSET) <= PW_WORKOUT_COOLDOWN_MS &&
           get32(r + TAKEOUT_OFFSET) <= PW_TAKEOUT_COOLDOWN_MS;
}
static bool snapshot_ready(const pet_world_t *w)
{
    return w->phase != PW_ACTION && w->phase != PW_TRANSITION && w->phase != PW_MINIGAME &&
           !(w->phase == PW_WALK && w->purpose != 0);
}
bool ps_visible_saved(const pet_save_t *s, const pet_world_t *w)
{
    return s->enabled && s->has_saved && snapshot_ready(w) &&
           s->saved[12] == w->food && s->saved[13] == w->mood &&
           s->saved[14] == (uint8_t)w->scene && s->saved[15] == (w->phase == PW_SLEEP) &&
           order_state(s->saved) == w->order &&
           (s->saved[4] == SCHEMA
                ? get32(s->saved + 46) == w->journal_page &&
                  get16(s->saved + 50) == w->journal_stamps &&
                  get16(s->saved + 52) == w->memory_wins &&
                  s->saved[54] == w->journal_progress && s->saved[55] == w->journal_badges
                : w->journal_page == 1 && !w->journal_stamps && !w->memory_wins &&
                  !w->journal_progress && !w->journal_badges);
}
static uint32_t remaining(const pet_world_t *w, pw_command_t action)
{
    return w->cooldown[action] > w->now ? (uint32_t)(w->cooldown[action] - w->now) : 0;
}
static bool capture(const pet_world_t *w, uint8_t *r, uint32_t seq)
{
    /* Never persist an unfinished business intent or a half-swapped scene. */
    if (!snapshot_ready(w)) return false;
    memset(r, 0, 64);
    put32(r, MAGIC);
    r[4] = SCHEMA;
    r[6] = 64;
    put32(r + 8, seq);
    r[12] = w->food; r[13] = w->mood; r[14] = (uint8_t)w->scene; r[15] = w->phase == PW_SLEEP;
    put32(r + 16, (uint32_t)w->food_elapsed);
    put32(r + 20, (uint32_t)w->mood_elapsed);
    put32(r + 24, (uint32_t)w->rest_elapsed);
    for (unsigned i = 0; i < sizeof(saved_actions) / sizeof(saved_actions[0]); i++)
    {
        uint32_t value = remaining(w, saved_actions[i]);
        if (value > saved_limits[i]) return false;
        put16(r + 32 + 2 * i, (uint16_t)value);
    }
    put32(r + 46, w->journal_page);
    put16(r + 50, w->journal_stamps);
    put16(r + 52, w->memory_wins);
    r[54] = w->journal_progress;
    r[55] = w->journal_badges;
    uint32_t order = 0;
    if (w->order == PW_ORDER_READY)
        order = 1;
    else if (w->order == PW_ORDER_DELIVERING)
    {
        if (w->order_due <= w->now || w->order_due - w->now > PW_DELIVERY_MS)
            return false;
        order = 1U + (uint32_t)(w->order_due - w->now);
    }
    put32(r + ORDER_OFFSET, order);

    put32(r + 56, crc(r, 56)); put32(r + 60, COMMIT);
    return valid(r);
}
static void restore(const uint8_t *r, pet_world_t *w)
{
    pw_init(w);
    w->food = r[12]; w->mood = r[13]; w->scene = (pw_scene_t)r[14];
    w->phase = r[15] ? PW_SLEEP : PW_IDLE;
    w->node = w->scene == PW_CAMPUS ? 0 : (r[15] ? 6 : 3);
    pw_point_t p = pw_node(w->scene, (unsigned)w->node);
    w->x = p.x; w->y = p.y;
    w->food_elapsed = get32(r + 16); w->mood_elapsed = get32(r + 20); w->rest_elapsed = get32(r + 24);
    w->order = order_state(r);
    if (w->order == PW_ORDER_DELIVERING)
        w->order_due = order_code(r) - 1;
    if (r[4] == SCHEMA)
    {
        for (unsigned i = 0; i < sizeof(saved_actions) / sizeof(saved_actions[0]); i++)
            w->cooldown[saved_actions[i]] = get16(r + 32 + 2 * i);
        w->journal_page = get32(r + 46);
        w->journal_stamps = get16(r + 50);
        w->memory_wins = get16(r + 52);
        w->journal_progress = r[54];
        w->journal_badges = r[55];
    }
    else
    {
        w->cooldown[PW_EAT] = get32(r + 32);
        w->cooldown[PW_TOUCH] = get32(r + 36);
        w->cooldown[PW_STUDY] = get32(r + 44);
        w->cooldown[PW_CODE] = get32(r + 48);
        if (r[4] == 3)
        {
            w->cooldown[PW_WORKOUT] = get32(r + WORKOUT_OFFSET);
            w->cooldown[PW_TAKEOUT] = get32(r + TAKEOUT_OFFSET);
        }
    }
}
ps_result_t ps_open(pet_save_t *s, ps_io_t io, pet_world_t *w)
{
    memset(s, 0, sizeof(*s)); s->io = io; s->sector = -1;
    if (!io.read || !io.write || !io.erase) return PS_INVALID;
    uint8_t r[64]; bool future = false;
    for (uint32_t offset = 0; offset < PS_AREA_SIZE; offset += 64)
    {
        if (!io.read(io.context, offset, r, 64)) return PS_IO_ERROR;
        if (committed(r) && !known_header(r))
            future = true;
        if (valid(r) && (!s->has_saved || newer(get32(r + 8), s->sequence)))
        {
            memcpy(s->saved, r, 64); s->sequence = get32(r + 8);
            s->sector = (int)(offset / PS_SECTOR_SIZE); s->has_saved = true;
        }
    }
    if (future) return PS_VERSION; /* Do not overwrite a newer firmware's data. */
    s->enabled = true;
    if (!s->has_saved) return PS_EMPTY;
    restore(s->saved, w);
    return PS_OK;
}
ps_result_t ps_save(pet_save_t *s, const pet_world_t *w)
{
    uint8_t r[64], check[64];
    if (!s->enabled) return PS_IO_ERROR;
    /* Reconcile a commit whose final read may have failed. Never reuse its sequence. */
    pet_save_t probe;
    pet_world_t ignored;
    ps_result_t scan = ps_open(&probe, s->io, &ignored);
    if (scan != PS_OK && scan != PS_EMPTY)
    {
        if (scan == PS_VERSION) s->enabled = false;
        return scan;
    }
    s->has_saved = probe.has_saved; s->sector = probe.sector; s->sequence = probe.sequence;
    memcpy(s->saved, probe.saved, 64);
    if (!capture(w, r, s->sequence + 1)) return PS_WAIT;
    if (s->has_saved && !memcmp(r + 12, s->saved + 12, 44)) return PS_UNCHANGED;
    unsigned sector = s->sector < 0 ? 0U : 1U - (unsigned)s->sector;
    uint32_t address = sector * PS_SECTOR_SIZE, slot;
    for (slot = 0; slot < PS_SECTOR_SIZE; slot += 64)
    {
        if (!s->io.read(s->io.context, address + slot, check, 64)) return PS_IO_ERROR;
        unsigned i = 0;
        while (i < 64 && check[i] == 255) i++;
        if (i == 64) break;
    }
    if (slot == PS_SECTOR_SIZE)
    {
        if (!s->io.erase(s->io.context, address, PS_SECTOR_SIZE)) return PS_IO_ERROR;
        slot = 0;
    }
    address += slot;
    /* Target is always in the sector opposite the newest committed record. */
    if (!s->io.write(s->io.context, address, r, 60) ||
        !s->io.read(s->io.context, address, check, 60) || memcmp(r, check, 60) ||
        !s->io.write(s->io.context, address + 60, r + 60, 4) ||
        !s->io.read(s->io.context, address, check, 64) || memcmp(r, check, 64)) return PS_IO_ERROR;
    memcpy(s->saved, r, 64); s->sequence++;
    s->sector = (int)sector; s->has_saved = true;
    return PS_OK;
}
ps_result_t ps_poll(pet_save_t *s, const pet_world_t *w)
{
    uint8_t r[64];
    if (!s->enabled) return PS_IO_ERROR;
    if (!capture(w, r, 0)) { s->pending = false; return PS_WAIT; }
    uint8_t key[15], saved[15];
    visible_key(r, key);
    visible_key(s->saved, saved);
    bool changed = !s->has_saved || memcmp(key, saved, sizeof(key)) != 0;
    if (changed)
    {
        if (!s->pending || memcmp(s->pending_key, key, sizeof(key)))
        {
            s->pending = true; s->dirty_since = w->now; memcpy(s->pending_key, key, sizeof(key));
        }
    }
    else s->pending = false;
    bool due = (s->pending && w->now - s->dirty_since >= 3000) || w->now - s->last_success >= 60000;
    if (!due || (s->attempted && w->now - s->last_attempt < 10000)) return PS_WAIT;
    s->last_attempt = w->now; s->attempted = true;
    ps_result_t result = ps_save(s, w);
    if (result == PS_OK || result == PS_UNCHANGED)
    {
        s->last_success = w->now; s->pending = false;
    }
    return result;
}
