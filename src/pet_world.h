#ifndef PET_WORLD_H
#define PET_WORLD_H
#include <stdbool.h>
#include <stdint.h>
#include "pet_memory.h"
#include "pet_challenge.h"
#include "pet_economy.h"
#include "pet_time.h"

enum
{
    PW_CAMPUS_WIDTH = 1400,
    PW_CAMPUS_HEIGHT = 1300,
    PW_ROAD_RADIUS = 32,
    PW_PATH_CAPACITY = 64
};

typedef enum
{
    PW_DORM,
    PW_CLASSROOM,
    PW_CANTEEN,
    PW_CAMPUS,
    PW_GYM
} pw_scene_t;
typedef enum
{
    PW_NONE,
    PW_EAT,
    PW_TOUCH,
    PW_REST,
    PW_STUDY,
    PW_CODE,
    PW_WAKE,
    PW_WORKOUT,
    PW_TAKEOUT,
    PW_MEMORY,
    PW_ACTION_COUNT,
    PW_ORDER = PW_ACTION_COUNT,
    PW_GO_DORM = 20,
    PW_GO_CLASSROOM,
    PW_GO_CANTEEN,
    PW_EXIT,
    PW_ENTER_DORM = 30,
    PW_ENTER_CLASSROOM,
    PW_ENTER_CANTEEN,
    PW_ENTER_GYM,
    PW_MEMORY_START = 40,
    PW_MEMORY_0,
    PW_MEMORY_1,
    PW_MEMORY_2,
    PW_MEMORY_3,
    PW_MEMORY_CANCEL,
    PW_JOURNAL_CLAIM = 50,
    PW_SOUND_NEXT = 60,
    PW_MUSIC_TOGGLE,
    PW_VOLUME_DOWN,
    PW_VOLUME_UP,
    PW_MEAL_GAME = 70,
    PW_RHYTHM_GAME,
    PW_FIND_GAME,
    PW_LIFE,
    PW_BUY_SNACK = 80,
    PW_BUY_PLANT,
    PW_BUY_PICTURE,
    PW_BUY_SHELF,
    PW_DAILY_CHECKIN,
    PW_DANCE = 90,
    PW_BADMINTON,
    PW_WORKSTUDY,
    PW_MEAL_FIRST = 110,
    PW_MEAL_LAST = PW_MEAL_FIRST + PE_MENU_COUNT - 1,
    PW_DELIVERY_FIRST = 120,
    PW_DELIVERY_LAST = PW_DELIVERY_FIRST + PE_MENU_COUNT - 1,
    PW_SIT_MEAL = 130,
    PW_MEAL_LEAVE
} pw_command_t;
typedef enum
{
    PW_IDLE,
    PW_WALK,
    PW_ACTION,
    PW_SLEEP,
    PW_TRANSITION,
    PW_MINIGAME,
    PW_SEATED
} pw_phase_t;
typedef enum
{
    PW_OK,
    PW_BUSY,
    PW_ASLEEP,
    PW_COOLDOWN,
    PW_FULL,
    PW_LOW_FOOD,
    PW_INVALID,
    PW_WRONG_ROOM,
    PW_ORDER_ACTIVE,
    PW_NO_ORDER,
    PW_NO_COINS,
    PW_CURFEW
} pw_result_t;
typedef enum
{
    PW_ORDER_NONE,
    PW_ORDER_DELIVERING,
    PW_ORDER_READY
} pw_order_t;
enum { PW_DELIVERY_MS = 30000, PW_TOUCH_COOLDOWN_MS = 10000, PW_WORKOUT_COOLDOWN_MS = 15000, PW_TAKEOUT_COOLDOWN_MS = 60000, PW_MEMORY_COOLDOWN_MS = 60000 };
enum { PW_ACTIVITY_COOLDOWN_MS = 30000, PW_WORK_COOLDOWN_MS = 60000, PW_WORK_COINS = 3, PW_DANCE_MOOD = 5, PW_BADMINTON_MOOD = 4 };
enum { PW_CHECKIN_COINS = 6, PW_CHECKIN_MIN_DAY = 19723, PW_CHECKIN_MAX_DAY = 24836 };
typedef enum { PW_CHECKIN_WAIT_TIME, PW_CHECKIN_READY, PW_CHECKIN_DONE, PW_CHECKIN_FUTURE, PW_CHECKIN_FULL } pw_checkin_t;

typedef enum
{
    PJ_STUDY, PJ_EAT, PJ_REST, PJ_WORKOUT, PJ_EXPLORE, PJ_CODE, PJ_TOUCH, PJ_COUNT
} pw_wish_t;

typedef struct
{
    float x, y;
} pw_point_t;
typedef struct
{
    pw_scene_t scene;
    pw_phase_t phase;
    pw_command_t action, target;
    int8_t node, edge_a, edge_b, route[PW_PATH_CAPACITY];
    uint8_t route_at, route_len, purpose, food, mood, facing;
    int8_t destination, entry_node, point_edge_a, point_edge_b;
    pw_point_t point_goal;
    bool swapped;
    float x, y;
    uint64_t now, action_start, action_end, transition_start;
    uint64_t cooldown[PW_ACTION_COUNT], food_elapsed, mood_elapsed, rest_elapsed, message_until;
    uint64_t order_due, gain_until;
    uint32_t settlements[PW_ACTION_COUNT];
    pw_order_t order;
    uint32_t journal_page;
    uint16_t journal_stamps, memory_wins;
    uint8_t journal_progress, journal_badges, sound_mode, volume;
    pm_game_t memory;
    pg_game_t challenge;
    int message, gain, mood_gain;
    pt_clock_t clock;
    uint8_t activity_kind; /* 0=original, 1=self-study, 2=snack, 3=dance, 4=badminton, 5=work */
    uint64_t activity_start;
    uint64_t activity_until;
    bool action_tired;
    pe_wallet_t wallet;
    uint16_t checkin_day;
    int coin_gain;
    uint64_t coin_until;
    uint32_t snacks;
    uint8_t meal_choice, order_choice;
    bool selected_meal;
    uint32_t curfew_events;
    uint16_t dishes; /* bit0-5 canteen menu, bit6-11 delivery menu; see pw_dish_count */
} pet_world_t;
typedef struct
{
    float x, y, scale, lift;
    uint8_t frame, facing;
    bool occlusion, sleeping, airborne;
} pw_visual_t;
void pw_init(pet_world_t *w);
void pw_advance(pet_world_t *w, uint32_t ms);
pw_result_t pw_request(pet_world_t *w, pw_command_t cmd);
void pw_visual(const pet_world_t *w, pw_visual_t *v);
bool pw_busy(const pet_world_t *w);
bool pw_curfew(const pet_world_t *w);
/* Number of unlocked dishes in the 12-bit album mask (0..12). */
unsigned pw_dish_count(const pet_world_t *w);
const char *pw_state_text(const pet_world_t *w);
const char *pw_message_text(const pet_world_t *w);
const char *pw_need_text(const pet_world_t *w);
pw_checkin_t pw_checkin_status(const pet_world_t *w);
const char *pw_life_text(pw_scene_t scene);
pw_result_t pw_walk_to(pet_world_t *w, float x, float y);
bool pw_room_ground(pw_scene_t scene, float x, float y);
void pw_camera(const pet_world_t *w, float *x, float *y);
unsigned pw_road_count(void);
void pw_road(unsigned index, pw_point_t *a, pw_point_t *b);
bool pw_campus_selftest(void (*report)(const char *, bool));
pw_wish_t pw_journal_wish(const pet_world_t *world, unsigned slot);
const char *pw_wish_text(pw_wish_t wish);
unsigned pw_campus_door(pw_scene_t scene);
pw_point_t pw_node(pw_scene_t scene, unsigned node);
uint32_t pw_duration(pw_command_t action);
bool pw_selftest(void (*report)(const char *, bool));
bool pw_economy_selftest(void (*report)(const char *, bool));
bool pw_life_selftest(void (*report)(const char *, bool));
#endif
