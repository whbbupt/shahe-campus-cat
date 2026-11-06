#ifndef PET_WORLD_H
#define PET_WORLD_H
#include <stdbool.h>
#include <stdint.h>

enum { PW_CAMPUS_WIDTH = 1120, PW_CAMPUS_HEIGHT = 1040, PW_ROAD_RADIUS = 32 };

typedef enum
{
    PW_DORM,
    PW_CLASSROOM,
    PW_CANTEEN,
    PW_CAMPUS
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
    PW_GO_DORM = 20,
    PW_GO_CLASSROOM,
    PW_GO_CANTEEN,
    PW_EXIT,
    PW_ENTER_DORM = 30,
    PW_ENTER_CLASSROOM,
    PW_ENTER_CANTEEN
} pw_command_t;
typedef enum
{
    PW_IDLE,
    PW_WALK,
    PW_ACTION,
    PW_SLEEP,
    PW_TRANSITION
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
    PW_WRONG_ROOM
} pw_result_t;
typedef struct
{
    float x, y;
} pw_point_t;
typedef struct
{
    pw_scene_t scene;
    pw_phase_t phase;
    pw_command_t action, target;
    int8_t node, edge_a, edge_b, route[32];
    uint8_t route_at, route_len, purpose, food, mood, facing;
    int8_t destination, entry_node, point_edge_a, point_edge_b;
    pw_point_t point_goal;
    bool swapped;
    float x, y;
    uint64_t now, action_start, action_end, transition_start;
    uint64_t cooldown[7], food_elapsed, mood_elapsed, rest_elapsed, message_until;
    uint32_t settlements[7];
    int message, gain;
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
const char *pw_state_text(const pet_world_t *w);
const char *pw_message_text(const pet_world_t *w);
pw_result_t pw_walk_to(pet_world_t *w, float x, float y);
bool pw_room_ground(pw_scene_t scene, float x, float y);
void pw_camera(const pet_world_t *w, float *x, float *y);
unsigned pw_road_count(void);
void pw_road(unsigned index, pw_point_t *a, pw_point_t *b);
bool pw_campus_selftest(void (*report)(const char *, bool));
pw_point_t pw_node(pw_scene_t scene, unsigned node);
uint32_t pw_duration(pw_command_t action);
bool pw_selftest(void (*report)(const char *, bool));
#endif
