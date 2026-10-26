#ifndef PET_WORLD_H
#define PET_WORLD_H
#include <stdbool.h>
#include <stdint.h>

typedef enum
{
    PW_DORM,
    PW_CLASSROOM,
    PW_CANTEEN
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
    PW_GO_CANTEEN
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
    int8_t node, edge_a, edge_b, route[12];
    uint8_t route_at, route_len, purpose, food, mood, facing;
    int8_t destination;
    bool swapped;
    float x, y;
    uint64_t now, action_start, action_end, transition_start, idle_until;
    uint64_t cooldown[7], food_elapsed, mood_elapsed, rest_elapsed, message_until;
    uint32_t settlements[7], random;
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
pw_point_t pw_node(pw_scene_t scene, unsigned node);
uint32_t pw_duration(pw_command_t action);
bool pw_selftest(void (*report)(const char *, bool));
#endif
