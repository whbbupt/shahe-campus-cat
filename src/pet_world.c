/* Fixed-size navigation and time-driven rules. No LVGL or RTOS dependency. */
#include "pet_world.h"
#include <float.h>
#include <math.h>
#include <string.h>

static const pw_point_t nodes[PW_GYM + 1][PW_PATH_CAPACITY] = {
    {{344, 247}, [3] = {189, 367}, [6] = {169, 307}, [7] = {105, 307}, [1] = {304, 302}},
    {{343, 242}, [3] = {188, 358}, [6] = {148, 298}},
    {{346, 239}, [3] = {184, 302}, [6] = {152, 300}},
    {
#define SHAHE_NODE(id, x, y) [id] = {x, y},
#define SHAHE_ROAD(a, b)
#include "pet_shahe_roads.inc"
#undef SHAHE_NODE
#undef SHAHE_ROAD
    },
    [PW_GYM] = {{345, 246}, [3] = {232, 359}, [6] = {187, 329}}};
static const uint8_t campus_edges[][2] = {
#define SHAHE_NODE(id, x, y)
#define SHAHE_ROAD(a, b) {a, b},
#include "pet_shahe_roads.inc"
#undef SHAHE_NODE
#undef SHAHE_ROAD
};
enum { ROOM_CORNERS = 8, ROOM_START = 24, ROOM_GOAL = 25, ROAD_GOAL = PW_PATH_CAPACITY - 1 };

typedef struct
{
    float left, top, right, bottom;
} floor_block_t;

/* These are foot-space obstacles, expanded slightly beyond the painted bases. */
static const floor_block_t blocks[PW_GYM + 1][4] = {
    {{23, 243, 178, 301}, {213, 276, 260, 324}, {22, 390, 59, 409}},
    {{42, 244, 134, 399}, {200, 244, 291, 319}, {314, 359, 371, 385}},
    {{34, 242, 142, 326}, {214, 242, 319, 326}, {89, 326, 202, 419}, {319, 350, 376, 374}},
    [PW_GYM] = {{34, 241, 168, 322}, {289, 348, 356, 372}}
};
static const unsigned block_count[] = {3, 3, 4, [PW_GYM] = 2};
static const unsigned floor_top[] = {246, 241, 236, [PW_GYM] = 242};

bool pw_room_ground(pw_scene_t scene, float x, float y)
{
    if ((unsigned)scene > PW_GYM || scene == PW_CAMPUS || !isfinite(x) || !isfinite(y) ||
        x < 12 || x > 378 || y < floor_top[scene] || y > 440)
        return false;
    for (unsigned i = 0; i < block_count[scene]; i++)
    {
        floor_block_t b = blocks[scene][i];
        if (x >= b.left && x <= b.right && y >= b.top && y <= b.bottom)
            return false;
    }
    return true;
}

static bool clip_axis(float start, float delta, float lo, float hi, float *enter, float *exit)
{
    if (fabsf(delta) < .0001f)
        return start >= lo && start <= hi;
    float first = (lo - start) / delta, last = (hi - start) / delta;
    *enter = fmaxf(*enter, fminf(first, last));
    *exit = fminf(*exit, fmaxf(first, last));
    return *enter <= *exit;
}

static bool clear_segment(pw_scene_t scene, pw_point_t a, pw_point_t b)
{
    for (unsigned i = 0; i < block_count[scene]; i++)
    {
        floor_block_t obstacle = blocks[scene][i];
        float enter = 0, exit = 1;
        if (clip_axis(a.x, b.x - a.x, obstacle.left, obstacle.right, &enter, &exit) &&
            clip_axis(a.y, b.y - a.y, obstacle.top, obstacle.bottom, &enter, &exit))
            return false;
    }
    return true;
}
static const uint32_t durations[PW_ACTION_COUNT] = {0, 4000, 1500, 600, 8000, 10000, 500, 8000, 3200, 0};
static const uint32_t cooldowns[PW_ACTION_COUNT] = {
    0, 2000, PW_TOUCH_COOLDOWN_MS, 0, 8000, 10000, 0, PW_WORKOUT_COOLDOWN_MS, PW_TAKEOUT_COOLDOWN_MS, PW_MEMORY_COOLDOWN_MS};
static const int food_delta[PW_ACTION_COUNT] = {0, 20, 0, 0, -7, -4, 0, -6, 12, 0};
static const int mood_delta[PW_ACTION_COUNT] = {0, 0, 3, 0, 4, 3, 0, 6, 0, 3};
static const pw_point_t supports[3][2] = {{{94, 154}, {104, 263}}, {{88, 294}, {0, 0}}, {{89, 298}, {0, 0}}};
enum
{
    MSG_NONE,
    MSG_WALK,
    MSG_TRAVEL,
    MSG_EAT,
    MSG_TOUCH,
    MSG_STUDY,
    MSG_CODE,
    MSG_REST,
    MSG_WAKE,
    MSG_DONE,
    MSG_BUSY,
    MSG_ASLEEP,
    MSG_COOLDOWN,
    MSG_FULL,
    MSG_LOW,
    MSG_INVALID,
    MSG_WORKOUT,
    MSG_ORDERED,
    MSG_DELIVERED,
    MSG_TAKEOUT,
    MSG_ORDER_ACTIVE,
    MSG_NO_ORDER,
    MSG_MEMORY,
    MSG_STAMP,
    MSG_POOR,
    MSG_PURCHASED,
    MSG_PLACED,
    MSG_STORED,
    MSG_CHECKIN,
    MSG_CHECKIN_DONE,
    MSG_CHECKIN_TIME,
    MSG_CHECKIN_FUTURE,
    MSG_CHECKIN_FULL,
    MSG_CURFEW
};
static float distance(float ax, float ay, float bx, float by)
{
    return hypotf(ax - bx, ay - by);
}
unsigned pw_campus_door(pw_scene_t scene)
{
    return scene == PW_GYM ? 52U : (unsigned)scene < PW_CAMPUS ? 13U + (unsigned)scene : 0U;
}
pw_point_t pw_node(pw_scene_t s, unsigned n)
{
    return (unsigned)s <= PW_GYM && n < PW_PATH_CAPACITY ? nodes[s][n] : (pw_point_t){0, 0};
}
uint32_t pw_duration(pw_command_t a)
{
    return a >= PW_EAT && a < PW_ACTION_COUNT ? durations[a] : 0;
}
bool pw_busy(const pet_world_t *w)
{
    return w->phase == PW_ACTION || w->phase == PW_SLEEP || w->phase == PW_TRANSITION || w->phase == PW_MINIGAME || w->phase == PW_SEATED;
}
bool pw_curfew(const pet_world_t *w)
{
    return w->clock.valid && w->clock.trusted && pt_bedtime(w->clock.utc) == 2;
}
static void say(pet_world_t *w, int msg)
{
    w->message = msg;
    w->message_until = w->now + 2400;
}
static uint8_t clamp_value(int x)
{
    return (uint8_t)(x < 0 ? 0 : (x > 100 ? 100 : x));
}
static void feedback(pet_world_t *w, int food, int mood, uint64_t at)
{
    w->gain = food;
    w->mood_gain = mood;
    w->gain_until = food || mood ? at + 2400 : 0;
}

pw_wish_t pw_journal_wish(const pet_world_t *w, unsigned slot)
{
    static const pw_wish_t wishes[3][3] = {
        {PJ_STUDY, PJ_EAT, PJ_REST}, {PJ_WORKOUT, PJ_EXPLORE, PJ_CODE}, {PJ_STUDY, PJ_WORKOUT, PJ_TOUCH}};
    return slot < 3 && w->journal_page ? wishes[(w->journal_page - 1) % 3][slot] : PJ_COUNT;
}
const char *pw_wish_text(pw_wish_t wish)
{
    static const char *names[] = {"学点新知识", "好好吃顿饭", "回床上休息",
                                   "去锻炼一下", "图书馆旁散步", "写一会代码", "和小猫贴贴"};
    return (unsigned)wish < PJ_COUNT ? names[wish] : "";
}
static void wish_done(pet_world_t *w, pw_wish_t wish)
{
    for (unsigned slot = 0; slot < 3; slot++)
        if (pw_journal_wish(w, slot) == wish)
            w->journal_progress |= (uint8_t)(1U << slot);
}

static void settle(pet_world_t *w, pw_command_t a)
{
    int old_food = w->food, old_mood = w->mood;
    int food=food_delta[a], mood=mood_delta[a];
    if (a == PW_MEMORY && w->challenge.kind == PG_LIFE) mood = 2;
    if(a==PW_STUDY && w->activity_kind==1){food=-4;mood=3;}
    if(a==PW_EAT && w->activity_kind==2){food=10;mood=1;}
    if (a == PW_EAT && w->selected_meal)
    { const pe_food_t *dish = pe_menu(false, w->meal_choice); food = dish->food; mood = dish->mood; }
    if (a == PW_TAKEOUT)
    { const pe_food_t *dish = pe_order(w->order_choice); food = dish->food; mood = dish->mood; }
    if(w->action_tired && (a==PW_STUDY || a==PW_CODE || a==PW_WORKOUT))mood=mood>2?mood-2:0;
    w->food = clamp_value(old_food + food);
    w->mood = clamp_value(old_mood + mood);
    w->settlements[a]++;
    if (a == PW_STUDY || (a == PW_MEMORY && w->challenge.kind == PG_NONE)) wish_done(w, PJ_STUDY);
    if (a == PW_EAT || a == PW_TAKEOUT) wish_done(w, PJ_EAT);
    if (a == PW_WORKOUT) wish_done(w, PJ_WORKOUT);
    if (a == PW_CODE) wish_done(w, PJ_CODE);
    if (a == PW_TOUCH) wish_done(w, PJ_TOUCH);
    feedback(w, w->food - old_food, w->mood - old_mood, w->now);
}

static void game_result(pet_world_t *w, bool won)
{
    w->coin_gain = 0;
    if (won)
    {
        settle(w, PW_MEMORY);
        if (w->memory_wins < UINT16_MAX) w->memory_wins++;
    }
    unsigned reward = won ? 10 : w->now - w->action_start >= 5000 ? 2 : 0;
    if (reward)
    {
        w->coin_gain = (int)pe_credit(&w->wallet, reward);
        w->coin_until = w->now + 2400;
        w->cooldown[PW_MEMORY] = w->now + PW_MEMORY_COOLDOWN_MS;
    }
}
static uint8_t heading(float dx, float dy)
{
    static const uint8_t map[8] = {6, 7, 0, 1, 2, 3, 4, 5};
    int oct = (int)floorf(atan2f(dy, dx) / .78539816339f + .5f);
    return map[(oct + 8) % 8];
}
static void idle(pet_world_t *w)
{
    w->phase = PW_IDLE;
    w->action = PW_NONE;
    w->purpose = 0;

}
void pw_init(pet_world_t *w)
{
    memset(w, 0, sizeof(*w));
    w->scene = PW_DORM;
    w->phase = PW_IDLE;
    w->journal_page = 1;
    w->wallet.coins = PE_START_COINS;
    w->sound_mode = 2;
    w->volume = 5;
    w->food = 70;
    w->mood = 60;
    w->node = 3;
    w->edge_a = w->edge_b = -1;
    w->destination = -1;
    w->point_edge_a = w->point_edge_b = -1;
    w->x = nodes[0][3].x;
    w->y = nodes[0][3].y;
    w->facing = 1;
}
unsigned pw_dish_count(const pet_world_t *w)
{
    unsigned mask = w ? w->dishes & 0x0FFFU : 0, count = 0;
    while (mask)
    {
        count += mask & 1U;
        mask >>= 1;
    }
    return count;
}
static void clock_step(pet_world_t *w, uint64_t dt)
{
    if (w->order == PW_ORDER_DELIVERING && w->now + dt >= w->order_due)
    {
        w->order = PW_ORDER_READY;
        w->order_due = 0;
        if (w->scene == PW_DORM && w->phase == PW_IDLE)
        {
            w->message = MSG_DELIVERED;
            w->message_until = w->now + dt + 2400;
        }
    }
    w->food_elapsed += dt;
    uint64_t n = w->food_elapsed / 300000U;
    w->food_elapsed %= 300000U;
    w->food = n >= w->food ? 0 : (uint8_t)(w->food - n);
    if (w->phase == PW_SLEEP)
    {
        w->rest_elapsed += dt;
        n = w->rest_elapsed / 60000U;
        w->rest_elapsed %= 60000U;
        n *= 3; /* Three mood points per minute asleep. */
        int old_mood = w->mood;
        w->mood = n >= 100U - w->mood ? 100 : (uint8_t)(w->mood + n);
        if (w->mood != old_mood)
            feedback(w, 0, w->mood - old_mood, w->now + dt);
    }
    else
    {
        w->mood_elapsed += dt;
        n = w->mood_elapsed / 600000U;
        w->mood_elapsed %= 600000U;
        w->mood = n >= w->mood ? 0 : (uint8_t)(w->mood - n);
    }
}
static pw_result_t eligible(const pet_world_t *w, pw_command_t a)
{
    if (a < PW_EAT || a > PW_ORDER || a == PW_WAKE)
        return PW_INVALID;
    if (w->phase == PW_SLEEP)
        return PW_ASLEEP;
    if (pw_busy(w))
        return PW_BUSY;
    if ((w->scene == PW_CAMPUS && a != PW_TOUCH) ||
        (a == PW_EAT && w->scene != PW_CANTEEN) ||
        (a == PW_STUDY && w->scene != PW_CLASSROOM) ||
        (a == PW_WORKOUT && w->scene != PW_GYM) ||
        ((a == PW_CODE || a == PW_REST || a == PW_ORDER || a == PW_TAKEOUT) && w->scene != PW_DORM))
        return PW_WRONG_ROOM;
    if (a == PW_MEMORY)
    {
        pg_kind_t kind = w->challenge.kind;
        pw_scene_t room = kind == PG_MEAL ? PW_CANTEEN : kind == PG_RHYTHM ? PW_GYM
            : kind == PG_FIND ? PW_DORM : PW_CLASSROOM;
        if (kind == PG_LIFE ? w->scene == PW_CAMPUS : w->scene != room) return PW_WRONG_ROOM;
    }
    if (a == PW_ORDER && w->order != PW_ORDER_NONE)
        return PW_ORDER_ACTIVE;
    if (a == PW_TAKEOUT && w->order != PW_ORDER_READY)
        return PW_NO_ORDER;
    if (w->cooldown[a == PW_ORDER ? PW_TAKEOUT : a] > w->now)
        return PW_COOLDOWN;
    if (((a == PW_EAT || a == PW_TAKEOUT || a == PW_ORDER) && w->food == 100) ||
        (a == PW_TOUCH && w->mood == 100))
        return PW_FULL;
    if (((a == PW_STUDY || a == PW_WORKOUT) && w->food < 10) ||
        (a == PW_CODE && w->food < 8))
        return PW_LOW_FOOD;
    if ((a == PW_EAT && w->wallet.coins < (w->selected_meal ? pe_menu(false, w->meal_choice)->price : pe_price(PE_MEAL))) ||
        (a == PW_ORDER && w->wallet.coins < pe_order(w->order_choice)->price)) return PW_NO_COINS;
    return PW_OK;
}
static pw_result_t reject(pet_world_t *w, pw_result_t r)
{
    static const int msgs[] = {MSG_NONE, MSG_BUSY, MSG_ASLEEP, MSG_COOLDOWN, MSG_FULL, MSG_LOW,
                               MSG_INVALID, MSG_INVALID, MSG_ORDER_ACTIVE, MSG_NO_ORDER, MSG_POOR, MSG_CURFEW};
    if (r == PW_CURFEW) w->curfew_events++;
    w->gain_until = 0;
    say(w, msgs[r]);
    return r;
}
static void begin(pet_world_t *w, pw_command_t a)
{
    if (a == PW_EAT && !pe_pay(&w->wallet, w->selected_meal ? pe_menu(false, w->meal_choice)->price : pe_price(PE_MEAL)))
    { w->target = PW_NONE; idle(w); reject(w, PW_NO_COINS); return; }
    static const int messages[PW_ACTION_COUNT] = {
        MSG_NONE, MSG_EAT, MSG_TOUCH, MSG_REST, MSG_STUDY, MSG_CODE, MSG_WAKE, MSG_WORKOUT, MSG_TAKEOUT, MSG_MEMORY};
    w->phase = a == PW_MEMORY && w->challenge.kind != PG_LIFE ? PW_MINIGAME : PW_ACTION;
    w->activity_kind = w->clock.valid && a==PW_STUDY && !pt_class_time(w->clock.utc) ? 1
        : w->clock.valid && a==PW_EAT && !w->selected_meal && !pt_meal_time(w->clock.utc) ? 2 : 0;
    w->action_tired=w->clock.valid && pt_bedtime(w->clock.utc)==2;
    w->action = a;
    w->target = PW_NONE;
    w->purpose = 0;
    w->route_len = w->route_at = 0;
    w->action_start = w->now;
    w->action_end = w->now + durations[a];
    w->gain_until = 0;
    if (a == PW_MEMORY)
    {
        uint32_t seed = (uint32_t)w->now ^ w->journal_page * 977U ^ w->memory_wins * 65537U;
        if (w->challenge.kind == PG_LIFE) w->action_end = w->now + 4000;
        else if (w->challenge.kind != PG_NONE) pg_start(&w->challenge, w->challenge.kind, seed);
        else pm_start(&w->memory, seed);
    }
    if (a != PW_REST && a != PW_WAKE && a != PW_WORKOUT && a != PW_TAKEOUT && a != PW_MEMORY)
        settle(w, a);
    say(w, messages[a]);
}
static pw_point_t point(const pet_world_t *w, int n)
{
    if (w->scene == PW_CAMPUS)
        return n == ROAD_GOAL ? w->point_goal : pw_node(w->scene, (unsigned)n);
    if (n == ROOM_START)
        return (pw_point_t){w->x, w->y};
    if (n == ROOM_GOAL)
        return w->point_goal;
    if (n >= ROOM_CORNERS)
    {
        unsigned k = (unsigned)(n - ROOM_CORNERS);
        floor_block_t b = blocks[w->scene][k / 4];
        return (pw_point_t){k & 1 ? b.right + 1 : b.left - 1, k & 2 ? b.bottom + 1 : b.top - 1};
    }
    return pw_node(w->scene, (unsigned)n);
}
unsigned pw_road_count(void)
{
    return sizeof(campus_edges) / sizeof(campus_edges[0]);
}
void pw_road(unsigned index, pw_point_t *a, pw_point_t *b)
{
    if (index >= pw_road_count())
    {
        *a = *b = (pw_point_t){0, 0};
        return;
    }
    *a = pw_node(PW_CAMPUS, campus_edges[index][0]);
    *b = pw_node(PW_CAMPUS, campus_edges[index][1]);
}
void pw_camera(const pet_world_t *w, float *x, float *y)
{
    *x = w->scene == PW_CAMPUS ? fmaxf(0, fminf(PW_CAMPUS_WIDTH - 390, w->x - 195)) : 0;
    *y = w->scene == PW_CAMPUS ? fmaxf(0, fminf(PW_CAMPUS_HEIGHT - 450, w->y - 230)) : 0;
}
static bool shortest(const pet_world_t *w, int start, int goal, int8_t *route, unsigned *len, float *cost)
{
    pw_scene_t s = w->scene;
    float dist[PW_PATH_CAPACITY];
    int8_t prev[PW_PATH_CAPACITY];
    bool done[PW_PATH_CAPACITY] = {false};
    int count = s == PW_CAMPUS ? ROAD_GOAL + 1 : ROOM_GOAL + 1;
    for (int i = 0; i < count; i++)
    {
        dist[i] = FLT_MAX;
        prev[i] = -1;
    }
    dist[start] = 0;
    for (int k = 0; k < count; k++)
    {
        int at = -1;
        for (int i = 0; i < count; i++)
            if (!done[i] && (at < 0 || dist[i] < dist[at]))
                at = i;
        if (at < 0 || dist[at] == FLT_MAX)
            break;
        done[at] = true;
        if (at == goal)
            break;
        int links = s == PW_CAMPUS ? (int)pw_road_count() + 2 : count;
        for (int e = 0; e < links; e++)
        {
            int next;
            if (s == PW_CAMPUS)
            {
                int a = e < (int)pw_road_count() ? campus_edges[e][0] : ROAD_GOAL;
                int b = e < (int)pw_road_count() ? campus_edges[e][1]
                          : e == (int)pw_road_count() ? w->point_edge_a : w->point_edge_b;
                if (a < 0 || b < 0)
                    continue;
                next = a == at ? b : (b == at ? a : -1);
            }
            else
            {
                next = e;
                bool corner = next >= ROOM_CORNERS &&
                              next < ROOM_CORNERS + (int)block_count[s] * 4;
                if (done[next] || (next != goal && !corner))
                    continue;
                pw_point_t a = point(w, at), b = point(w, next);
                if (!pw_room_ground(s, b.x, b.y) || !clear_segment(s, a, b))
                    continue;
            }
            if (next < 0 || done[next])
                continue;
            float d = dist[at] + distance(point(w, at).x, point(w, at).y, point(w, next).x, point(w, next).y);
            if (d < dist[next])
            {
                dist[next] = d;
                prev[next] = (int8_t)at;
            }
        }
    }
    if (dist[goal] == FLT_MAX)
        return false;
    int8_t rev[PW_PATH_CAPACITY];
    unsigned n = 0;
    for (int x = goal; x >= 0; x = prev[x])
    {
        rev[n++] = (int8_t)x;
        if (x == start)
            break;
        if (n == PW_PATH_CAPACITY)
            return false;
    }
    for (unsigned i = 0; i < n; i++)
        route[i] = rev[n - i - 1];
    *len = n;
    *cost = dist[goal];
    return true;
}
static void arrived(pet_world_t *w)
{
    unsigned purpose = w->purpose;
    if (w->scene == PW_CAMPUS && w->x > 825 && w->x < 1160 && w->y < 642 && w->y >= 395)
        wish_done(w, PJ_EXPLORE);
    w->purpose = 0;
    w->edge_a = w->edge_b = -1;
    if (purpose == 4)
    {
        w->phase = PW_SEATED; w->action = PW_EAT; w->target = PW_NONE;
        w->action_start = w->now; w->gain_until = 0;
    }
    else if (purpose == 1)
    {
        pw_command_t a = w->target;
        w->phase = PW_IDLE;
        pw_result_t r = eligible(w, a);
        if (r == PW_OK)
            begin(w, a);
        else
        {
            w->target = PW_NONE;
            idle(w);
            reject(w, r);
        }
    }
    else if (purpose == 2)
    {
        if (w->scene == PW_DORM && w->destination != PW_DORM && pw_curfew(w))
        { w->destination = -1; w->target = PW_NONE; idle(w); reject(w, PW_CURFEW); return; }
        w->phase = PW_TRANSITION;
        w->transition_start = w->now;
        w->swapped = false;
    }
    else
    {
        idle(w);
        if (w->scene == PW_CAMPUS && w->node == ROAD_GOAL)
        {
            w->edge_a = w->point_edge_a;
            w->edge_b = w->point_edge_b;
        }
    }
}
static void consume(pet_world_t *w)
{
    while (w->phase == PW_WALK && w->route_at < w->route_len)
    {
        int n = w->route[w->route_at];
        if (distance(w->x, w->y, point(w, n).x, point(w, n).y) > .02f)
            break;
        w->node = (int8_t)n;
        w->x = point(w, n).x;
        w->y = point(w, n).y;
        w->route_at++;
        w->edge_a = w->edge_b = -1;
    }
    if (w->phase == PW_WALK && w->route_at == w->route_len)
    {
        arrived(w);
        return;
    }
    if (w->phase == PW_WALK && w->edge_a < 0)
    {
        w->edge_a = w->node;
        w->edge_b = w->route[w->route_at];
    }
}
static bool move(pet_world_t *w, int goal, unsigned purpose)
{
    if (w->scene == PW_CAMPUS && (w->edge_a == ROAD_GOAL || w->edge_b == ROAD_GOAL))
    {
        w->edge_a = w->point_edge_a;
        w->edge_b = w->point_edge_b;
    }
    int anchor[2] = {ROOM_START, -1};
    if (w->scene == PW_CAMPUS)
    {
        anchor[0] = w->edge_a >= 0 ? w->edge_a : w->node;
        anchor[1] = w->edge_b;
    }
    int8_t best[PW_PATH_CAPACITY];
    unsigned best_n = 0;
    float best_cost = FLT_MAX;
    for (int k = 0; k < 2; k++)
        if (anchor[k] >= 0)
        {
            int8_t route[PW_PATH_CAPACITY];
            unsigned n;
            float cost;
            if (shortest(w, anchor[k], goal, route, &n, &cost))
            {
                cost += distance(w->x, w->y, point(w, anchor[k]).x, point(w, anchor[k]).y);
                if (cost < best_cost)
                {
                    best_cost = cost;
                    best_n = n;
                    memcpy(best, route, n);
                }
            }
        }
    if (!best_n)
        return false;
    memcpy(w->route, best, best_n);
    w->route_at = 0;
    w->route_len = (uint8_t)best_n;
    w->phase = PW_WALK;
    w->action = PW_NONE;
    w->purpose = (uint8_t)purpose;
    consume(w);
    return true;
}
pw_result_t pw_walk_to(pet_world_t *w, float x, float y)
{
    if (w->scene != PW_CAMPUS)
    {
        if (!pw_room_ground(w->scene, x, y))
            return PW_INVALID;
        if (pw_busy(w))
            return reject(w, w->phase == PW_SLEEP ? PW_ASLEEP : PW_BUSY);
        w->point_goal = (pw_point_t){x, y};
        w->target = PW_NONE;
        w->challenge.kind = PG_NONE;
        w->destination = -1;
        return move(w, ROOM_GOAL, 3) ? PW_OK : PW_INVALID;
    }
    if (!isfinite(x) || !isfinite(y) || x < 0 || y < 0 || x > PW_CAMPUS_WIDTH || y > PW_CAMPUS_HEIGHT)
        return PW_INVALID;
    if (pw_busy(w))
        return reject(w, PW_BUSY);
    float best = FLT_MAX;
    pw_point_t goal = {0, 0};
    int edge = -1;
    for (unsigned i = 0; i < pw_road_count(); i++)
    {
        pw_point_t a, b;
        pw_road(i, &a, &b);
        float dx = b.x - a.x, dy = b.y - a.y;
        float t = fmaxf(0, fminf(1, ((x - a.x) * dx + (y - a.y) * dy) / (dx * dx + dy * dy)));
        pw_point_t p = {a.x + t * dx, a.y + t * dy};
        float d = distance(x, y, p.x, p.y);
        if (d < best)
        {
            best = d;
            goal = p;
            edge = (int)i;
        }
    }
    if (best > PW_ROAD_RADIUS || edge < 0)
        return PW_INVALID;
    if (w->edge_a == ROAD_GOAL || w->edge_b == ROAD_GOAL)
    {
        w->edge_a = w->point_edge_a;
        w->edge_b = w->point_edge_b;
    }
    bool same_edge = (w->edge_a == campus_edges[edge][0] && w->edge_b == campus_edges[edge][1]) ||
                     (w->edge_b == campus_edges[edge][0] && w->edge_a == campus_edges[edge][1]);
    w->point_goal = goal;
    w->point_edge_a = campus_edges[edge][0];
    w->point_edge_b = campus_edges[edge][1];
    w->target = PW_NONE;
    w->destination = -1;
    if (same_edge)
    {
        w->route[0] = ROAD_GOAL;
        w->route_at = 0;
        w->route_len = 1;
        w->phase = PW_WALK;
        w->action = PW_NONE;
        w->purpose = 3;
        consume(w);
        return PW_OK;
    }
    return move(w, ROAD_GOAL, 3) ? PW_OK : PW_INVALID;
}
static uint16_t local_day(const pet_world_t *w)
{
    pt_date_t date;
    if (!w->clock.valid || !w->clock.trusted || !pt_decode(w->clock.utc, &date)) return 0;
    return (uint16_t)(((uint64_t)w->clock.utc + 8U * 3600U) / 86400U);
}
pw_checkin_t pw_checkin_status(const pet_world_t *w)
{
    unsigned day = local_day(w);
    if (!day) return PW_CHECKIN_WAIT_TIME;
    if (w->checkin_day > day) return PW_CHECKIN_FUTURE;
    if (w->checkin_day == day) return PW_CHECKIN_DONE;
    if (w->wallet.coins == PE_MAX_COINS) return PW_CHECKIN_FULL;
    return PW_CHECKIN_READY;
}
static pw_result_t request_base(pet_world_t *w, pw_command_t cmd)
{
    if (cmd == PW_DAILY_CHECKIN)
    {
        if (w->phase != PW_IDLE) return reject(w, w->phase == PW_SLEEP ? PW_ASLEEP : PW_BUSY);
        pw_checkin_t status = pw_checkin_status(w);
        w->coin_until = 0;
        w->gain_until = 0;
        if (status != PW_CHECKIN_READY)
        {
            say(w, status == PW_CHECKIN_DONE ? MSG_CHECKIN_DONE : status == PW_CHECKIN_FUTURE ? MSG_CHECKIN_FUTURE
                : status == PW_CHECKIN_FULL ? MSG_CHECKIN_FULL : MSG_CHECKIN_TIME);
            return status == PW_CHECKIN_DONE ? PW_COOLDOWN : status == PW_CHECKIN_FULL ? PW_FULL : PW_INVALID;
        }
        w->coin_gain = (int)pe_credit(&w->wallet, PW_CHECKIN_COINS);
        w->checkin_day = local_day(w);
        w->coin_until = w->now + 2400;
        say(w, MSG_CHECKIN);
        return PW_OK;
    }
    if (cmd >= PW_BUY_SNACK && cmd <= PW_BUY_SHELF)
    {
        if (w->phase != PW_IDLE) return reject(w, w->phase == PW_SLEEP ? PW_ASLEEP : PW_BUSY);
        if (cmd == PW_BUY_SNACK)
        {
            if (w->food == 100) return reject(w, PW_FULL);
            if (!pe_pay(&w->wallet, pe_price(PE_SNACK))) return reject(w, PW_NO_COINS);
            int food = w->food, mood = w->mood;
            w->food = clamp_value(food + 10); w->mood = clamp_value(mood + 1);
            w->snacks++;
            feedback(w, w->food - food, w->mood - mood, w->now);
            say(w, MSG_EAT);
        }
        else
        {
            if (w->scene != PW_DORM) return reject(w, PW_WRONG_ROOM);
            pe_result_t result = pe_furniture(&w->wallet, (pe_item_t)(PE_PLANT + cmd - PW_BUY_PLANT));
            if (result == PE_POOR) return reject(w, PW_NO_COINS);
            if (result == PE_INVALID) return reject(w, PW_INVALID);
            w->gain_until = 0;
            say(w, result == PE_BOUGHT ? MSG_PURCHASED : result == PE_PLACED ? MSG_PLACED : MSG_STORED);
        }
        w->coin_until = 0;
        return PW_OK;
    }

    if (cmd >= PW_MEAL_GAME && cmd <= PW_LIFE)
    {
        if (pw_busy(w)) return reject(w, w->phase == PW_SLEEP ? PW_ASLEEP : PW_BUSY);
        pg_kind_t kind = (pg_kind_t)(PG_MEAL + cmd - PW_MEAL_GAME);
        pg_kind_t previous = w->challenge.kind;
        w->challenge.kind = kind;
        pw_result_t r = eligible(w, PW_MEMORY);
        if (r != PW_OK) { w->challenge.kind = previous; return reject(w, r); }
        w->memory.phase = PM_OFF;
        w->challenge.phase = PG_OFF;
        w->destination = -1;
        w->target = PW_MEMORY;
        static const pw_point_t corners[] = {{68, 412}, {300, 390}, {304, 390}, {0, 0}, {277, 389}};
        int goal = 6;
        if (kind == PG_LIFE) { w->point_goal = corners[w->scene]; goal = ROOM_GOAL; }
        if (!move(w, goal, 1)) { w->challenge.kind = previous; w->target = PW_NONE; return reject(w, PW_INVALID); }
        say(w, MSG_WALK);
        return PW_OK;
    }
    if (cmd == PW_VOLUME_DOWN || cmd == PW_VOLUME_UP)
    {
        if (cmd == PW_VOLUME_UP && w->volume < 10) w->volume++;
        if (cmd == PW_VOLUME_DOWN && w->volume > 0) w->volume--;
        if (w->volume && !w->sound_mode) w->sound_mode = 1;
        return PW_OK;
    }
    if (cmd == PW_MUSIC_TOGGLE)
    {
        w->sound_mode = w->sound_mode == 2 ? 1 : 2;
        return PW_OK;
    }
    if (cmd == PW_SOUND_NEXT)
    {
        w->sound_mode = w->sound_mode ? w->sound_mode - 1 : 2;
        return PW_OK;
    }
    if (cmd == PW_MEMORY_CANCEL)
    {
        if (w->phase != PW_MINIGAME)
            return PW_INVALID;
        w->memory.phase = PM_OFF;
        w->challenge.phase = PG_OFF;
        idle(w);
        return PW_OK;
    }
    if (cmd >= PW_MEMORY_0 && cmd <= PW_MEMORY_3)
    {
        if (w->phase != PW_MINIGAME)
            return PW_INVALID;
        if (w->challenge.kind != PG_NONE)
        {
            pg_phase_t before = w->challenge.phase;
            pg_press(&w->challenge, (unsigned)(cmd - PW_MEMORY_0));
            if (before == PG_INPUT && (w->challenge.phase == PG_WON || w->challenge.phase == PG_LOST))
                game_result(w, w->challenge.phase == PG_WON);
            return PW_OK;
        }
        pm_phase_t before = w->memory.phase;
        pm_press(&w->memory, (unsigned)(cmd - PW_MEMORY_0));
        if (before == PM_INPUT && (w->memory.phase == PM_WON || w->memory.phase == PM_LOST))
            game_result(w, w->memory.phase == PM_WON);
        return PW_OK;
    }
    if (cmd == PW_JOURNAL_CLAIM) return reject(w, PW_INVALID); /* Retired product goal. */
    if (cmd == PW_MEMORY_START || cmd == PW_MEMORY)
    {
        if (pw_busy(w)) return reject(w, w->phase == PW_SLEEP ? PW_ASLEEP : PW_BUSY);
        pg_kind_t previous = w->challenge.kind;
        w->challenge.kind = PG_NONE;
        pw_result_t r = eligible(w, PW_MEMORY);
        if (r != PW_OK) { w->challenge.kind = previous; return reject(w, r); }
        cmd = PW_MEMORY;
    }
    if (cmd == PW_EXIT || (cmd >= PW_ENTER_DORM && cmd <= PW_ENTER_GYM))
    {
        if (pw_busy(w))
            return reject(w, w->phase == PW_SLEEP ? PW_ASLEEP : PW_BUSY);
        if ((cmd == PW_EXIT) == (w->scene == PW_CAMPUS))
            return reject(w, PW_WRONG_ROOM);
        if (w->scene == PW_DORM && cmd == PW_EXIT && pw_curfew(w)) return reject(w, PW_CURFEW);
        pw_scene_t next = cmd == PW_EXIT ? PW_CAMPUS
                          : cmd == PW_ENTER_GYM ? PW_GYM : (pw_scene_t)(cmd - PW_ENTER_DORM);
        int goal = cmd == PW_EXIT ? 0 : (int)pw_campus_door(next);
        w->entry_node = cmd == PW_EXIT ? (int8_t)pw_campus_door(w->scene) : 0;
        w->destination = (int8_t)next;
        w->target = PW_NONE;
        w->challenge.kind = PG_NONE;
        if (!move(w, goal, 2))
            return reject(w, PW_INVALID);
        say(w, MSG_TRAVEL);
        return PW_OK;
    }
    if (cmd >= PW_GO_DORM && cmd <= PW_GO_CANTEEN)
    {
        w->entry_node = 0;
        int next = cmd - PW_GO_DORM;
        if (next == w->scene)
        {
            if (w->phase == PW_WALK && w->purpose == 2)
            {
                w->route_len = w->route_at = 0;
                w->destination = -1;
                idle(w);
            }
            return PW_OK;
        }
        if (pw_busy(w))
            return reject(w, w->phase == PW_SLEEP ? PW_ASLEEP : PW_BUSY);
        if (w->scene == PW_DORM && pw_curfew(w)) return reject(w, PW_CURFEW);
        w->destination = (int8_t)next;
        w->target = PW_NONE;
        w->challenge.kind = PG_NONE;
        if (!move(w, 0, 2))
            return reject(w, PW_INVALID);
        say(w, MSG_TRAVEL);
        return PW_OK;
    }
    if (cmd == PW_REST && w->phase == PW_SLEEP)
    {
        begin(w, PW_WAKE);
        return PW_OK;
    }
    pw_result_t r = eligible(w, cmd);
    if (r != PW_OK)
        return reject(w, r);
    if (cmd == PW_ORDER)
    {
        if (!pe_pay(&w->wallet, pe_order(w->order_choice)->price)) return reject(w, PW_NO_COINS);
        w->order = PW_ORDER_DELIVERING;
        w->order_due = w->now + PW_DELIVERY_MS;
        w->gain_until = 0;
        say(w, MSG_ORDERED);
        return PW_OK;
    }
    if (w->phase == PW_WALK && w->purpose == 1 && w->target == cmd)
        return PW_OK;
    w->destination = -1;
    if (cmd == PW_TOUCH)
    {
        w->challenge.kind = PG_NONE;
        w->route_len = w->route_at = 0;
        begin(w, cmd);
        return PW_OK;
    }
    if (cmd != PW_MEMORY) w->challenge.kind = PG_NONE;
    w->target = cmd;
    if (!move(w, cmd == PW_CODE ? 7 : (cmd == PW_TAKEOUT ? 1 : 6), 1))
        return reject(w, PW_INVALID);
    say(w, MSG_WALK);
    return PW_OK;
}
pw_result_t pw_request(pet_world_t *w, pw_command_t cmd)
{
    if (cmd == PW_MEAL_LEAVE)
    {
        if (w->phase != PW_SEATED) return reject(w, PW_INVALID);
        idle(w); return PW_OK;
    }
    if (cmd == PW_SIT_MEAL)
    {
        if (pw_busy(w)) return reject(w, w->phase == PW_SLEEP ? PW_ASLEEP : PW_BUSY);
        if (w->scene != PW_CANTEEN) return reject(w, PW_WRONG_ROOM);
        w->target = PW_NONE; w->destination = -1; w->selected_meal = false;
        if (!move(w, 6, 4)) return reject(w, PW_INVALID);
        return PW_OK;
    }
    if ((cmd >= PW_MEAL_FIRST && cmd <= PW_MEAL_LAST) || cmd == PW_EAT)
    {
        uint8_t old = w->meal_choice; bool selected = w->selected_meal;
        if (w->phase == PW_SEATED)
        {
            if (w->now - w->action_start < 500) return reject(w, PW_BUSY);
            w->phase = PW_IDLE;
            w->selected_meal = cmd != PW_EAT;
            w->meal_choice = cmd == PW_EAT ? 0 : (uint8_t)(cmd - PW_MEAL_FIRST);
            pw_result_t result = eligible(w, PW_EAT);
            if (result == PW_OK)
            { begin(w, PW_EAT); w->action_start = w->now - 500; w->action_end = w->action_start + durations[PW_EAT]; }
            else
            { w->phase = PW_SEATED; w->meal_choice = old; w->selected_meal = selected; reject(w, result); }
            return result;
        }
        /* Never change a meal already being consumed. */
        if (pw_busy(w)) return reject(w, w->phase == PW_SLEEP ? PW_ASLEEP : PW_BUSY);
        w->selected_meal = cmd != PW_EAT;
        w->meal_choice = cmd == PW_EAT ? 0 : (uint8_t)(cmd - PW_MEAL_FIRST);
        pw_result_t result = request_base(w, PW_EAT);
        if (result != PW_OK) { w->meal_choice = old; w->selected_meal = selected; }
        return result;
    }
    if ((cmd >= PW_DELIVERY_FIRST && cmd <= PW_DELIVERY_LAST) || cmd == PW_ORDER)
    {
        uint8_t old = w->order_choice;
        w->order_choice = cmd == PW_ORDER ? 0 : (uint8_t)(cmd - PW_DELIVERY_FIRST + 1);
        pw_result_t result = request_base(w, PW_ORDER);
        if (result != PW_OK) w->order_choice = old;
        return result;
    }
    return request_base(w, cmd);
}
static void finish(pet_world_t *w)
{
    pw_command_t a = w->action;
    w->action = PW_NONE;
    /* Album unlocks only after a successful action completes, never on order/cancel. */
    if (a == PW_EAT && w->meal_choice < PE_MENU_COUNT)
        w->dishes |= (uint16_t)(1U << w->meal_choice);
    else if (a == PW_TAKEOUT && w->order_choice >= 1 && w->order_choice <= PE_MENU_COUNT)
        w->dishes |= (uint16_t)(1U << (PE_MENU_COUNT + w->order_choice - 1));
    if (a == PW_REST)
    {
        w->phase = PW_SLEEP;
        wish_done(w, PJ_REST);
        w->rest_elapsed = 0;
        say(w, MSG_REST);
        return;
    }
    if (a == PW_WAKE)
    {
        w->rest_elapsed = 0;
        idle(w);
        say(w, MSG_WAKE);
        return;
    }
    if (a == PW_WORKOUT || a == PW_TAKEOUT || a == PW_MEMORY)
        settle(w, a);
    if (a == PW_TAKEOUT)
    { w->order = PW_ORDER_NONE; w->order_choice = 0; }
    w->cooldown[a] = w->now + cooldowns[a];
    idle(w);
    say(w, MSG_DONE);
}
void pw_advance(pet_world_t *w, uint32_t ms)
{
    uint64_t left = ms;
    unsigned guard = 0;
    while (left && guard++ < 512)
    {
        consume(w);
        uint64_t until = left;
        if (w->phase == PW_WALK)
        {
            int n = w->route[w->route_at];
            float d = distance(w->x, w->y, point(w, n).x, point(w, n).y);
            until = (uint64_t)ceilf(d / 70.0f * 1000.0f);
            if (!until)
                until = 1;
        }
        else if (w->phase == PW_MINIGAME)
            until = w->challenge.kind == PG_NONE ? pm_remaining(&w->memory) : pg_remaining(&w->challenge);
        else if (w->phase == PW_ACTION)
            until = w->action_end - w->now;
        else if (w->phase == PW_TRANSITION)
            until = w->transition_start + (w->swapped ? 400 : 200) - w->now;
        uint64_t dt = left < until ? left : until;
        if (!dt)
            dt = 1;
        if (dt > left)
            dt = left;
        clock_step(w, dt);
        w->now += dt;
        left -= dt;
        if (w->phase == PW_WALK)
        {
            int n = w->route[w->route_at];
            float dx = point(w, n).x - w->x, dy = point(w, n).y - w->y, d = hypotf(dx, dy);
            float fraction = d > .001f ? 70.0f * (float)dt / 1000.0f / d : 1;
            w->facing = heading(dx, dy);
            if (fraction >= 1)
            {
                w->x = point(w, n).x;
                w->y = point(w, n).y;
            }
            else
            {
                w->x += dx * fraction;
                w->y += dy * fraction;
            }
            consume(w);
        }
        else if (w->phase == PW_MINIGAME)
        {
            if (w->challenge.kind == PG_NONE)
            {
                pm_advance(&w->memory, (uint32_t)dt);
                if (w->memory.phase == PM_OFF) idle(w);
            }
            else
            {
                pg_phase_t before = w->challenge.phase;
                pg_advance(&w->challenge, (uint32_t)dt);
                if (before == PG_INPUT && (w->challenge.phase == PG_WON || w->challenge.phase == PG_LOST))
                    game_result(w, w->challenge.phase == PG_WON);
                if (w->challenge.phase == PG_OFF) idle(w);
            }
        }
        else if (w->phase == PW_ACTION && w->now >= w->action_end)
            finish(w);
        else if (w->phase == PW_TRANSITION)
        {
            if (!w->swapped && w->now >= w->transition_start + 200)
            {
                if (w->scene == PW_DORM && w->destination != PW_DORM && pw_curfew(w))
                { w->destination = -1; idle(w); reject(w, PW_CURFEW); continue; }
                w->scene = (pw_scene_t)w->destination;
                w->node = w->scene == PW_CAMPUS ? w->entry_node : 0;
                w->x = point(w, w->node).x;
                w->y = point(w, w->node).y;
                w->point_edge_a = w->point_edge_b = -1;
                w->edge_a = w->edge_b = -1;
                w->swapped = true;
            }
            else if (w->swapped && w->now >= w->transition_start + 400)
            {
                w->destination = -1;
                if (w->scene == PW_CAMPUS)
                    idle(w);
                else
                    move(w, 3, 0);
            }
        }

    }
    if (left)
    {
        clock_step(w, left);
        w->now += left;
        w->route_len = w->route_at = 0;
        idle(w);
    }
}
void pw_visual(const pet_world_t *w, pw_visual_t *v)
{
    memset(v, 0, sizeof(*v));
    v->x = w->x;
    v->y = w->y;
    v->facing = w->facing;
    v->scale = fmaxf(.91f, fminf(1.05f, .92f + (w->y - 240) / 1000));
    if (w->scene == PW_CAMPUS)
        v->scale = .78f;
    if (w->phase == PW_ACTION && w->action == PW_MEMORY)
    {
        v->frame = (uint8_t)(w->facing * 2 + w->now / 500 % 2);
        return;
    }
    if (w->phase == PW_ACTION && w->action == PW_WORKOUT)
    {
        float enter = (float)(w->now - w->action_start) / 500.0f;
        float leave = (float)(w->action_end - w->now) / 500.0f;
        float amount = fminf(1, fminf(enter, leave));
        v->x += (107 - w->x) * amount;
        v->y += (285 - w->y) * amount;
        v->facing = 4;
        v->frame = (uint8_t)(v->facing * 2 + w->now / 115 % 2);
        return;
    }
    if (w->phase == PW_ACTION && w->action == PW_TAKEOUT)
    {
        v->frame = (uint8_t)(16 + w->now / 210 % 2);
        return;
    }
    bool furniture = w->phase == PW_SLEEP || w->phase == PW_SEATED || (w->phase == PW_ACTION && w->action != PW_TOUCH);
    bool settled = false;
    if (furniture)
    {
        bool bed = w->phase == PW_SLEEP || w->action == PW_REST || w->action == PW_WAKE;
        pw_point_t target = supports[w->scene][!bed && w->scene == PW_DORM ? 1 : 0];
        float progress = 1, amount = 1;
        int mode = 0;
        uint64_t elapsed = w->now - w->action_start;
        uint32_t enter = bed ? 600 : 500;
        if (w->phase == PW_ACTION || w->phase == PW_SEATED)
        {
            if (w->action == PW_WAKE)
            {
                mode = 2;
                progress = (float)elapsed / 500;
            }
            else if (elapsed < enter)
            {
                mode = 1;
                progress = (float)elapsed / enter;
            }
            else if (w->phase == PW_ACTION && w->action != PW_REST && elapsed > durations[w->action] - 500)
            {
                mode = 2;
                progress = (float)(elapsed - durations[w->action] + 500) / 500;
            }
        }
        float ease = progress * progress * (3 - 2 * progress);
        amount = mode == 2 ? 1 - ease : (mode == 1 ? ease : 1);
        v->lift = mode ? sinf(3.141592654f * progress) * (bed ? 23 : 17) : 0;
        if (bed)
        {
            /* Climb at the ladder, then turn in onto the upper bunk. */
            float climb = fminf(amount / .72f, 1);
            float onto_bed = fmaxf((amount - .72f) / .28f, 0);
            v->x += (target.x - w->x) * onto_bed;
            v->y += (target.y - w->y) * climb;
            v->lift = 0;
        }
        else
        {
            v->x += (target.x - w->x) * amount;
            v->y += (target.y - w->y) * amount - v->lift;
        }
        settled = !mode || (mode == 1 && progress >= .88f);
        v->airborne = mode != 0;
        float scale = bed ? 1 : (w->scene == PW_DORM ? .94f : (w->scene == PW_CLASSROOM ? 1.02f : .98f));
        v->scale += (scale - v->scale) * amount;
        v->sleeping = bed && settled;
        v->occlusion = !bed && amount > .35f;
        if (settled)
            v->facing = 5;
        else
            v->facing = mode == 2 ? heading(w->x - target.x, w->y - target.y)
                                  : heading(target.x - w->x, target.y - w->y);
    }
    v->frame = (uint8_t)(v->facing * 2 + ((w->phase == PW_WALK && w->now / 210 % 2) ? 1 : 0));
    if (settled)
        v->frame = v->sleeping ? 18 : (uint8_t)(16 + (w->action == PW_CODE && w->now / 210 % 2 ? 1 : 0));
}
const char *pw_state_text(const pet_world_t *w)
{
    if (w->phase == PW_SEATED) return "坐好啦，选一份饭吧";
    if (w->phase == PW_ACTION && w->action == PW_MEMORY) return pw_life_text(w->scene);
    if(w->phase==PW_IDLE && w->clock.valid && pt_bedtime(w->clock.utc))
        return pt_bedtime(w->clock.utc)==2 ? "有点困，回床上休息吧" : "准备睡觉啦 · 23点前休息";
    if(w->phase==PW_ACTION && w->action==PW_STUDY && w->activity_kind==1)return "安静自习中";
    if(w->phase==PW_ACTION && w->action==PW_EAT && w->activity_kind==2)return "吃份暖暖的简餐";
    if (w->phase == PW_MINIGAME)
        return w->challenge.kind == PG_MEAL ? "配餐中" : w->challenge.kind == PG_RHYTHM ? "跟着节拍动一动"
            : w->challenge.kind == PG_FIND ? "找东西中" : "记忆题中";
    if (w->phase == PW_SLEEP)
        return "睡觉中";
    if (w->phase == PW_TRANSITION)
        return "切换场景";
    if (w->phase == PW_WALK)
        return w->purpose == 2 ? "前往门口" : (w->purpose == 1 ? "走向家具" : "散步中");
    if (w->phase == PW_ACTION)
    {
        static const char *names[] = {"闲逛中", "吃饭中", "摸摸", "准备睡觉", "上课中", "敲代码", "起床中", "锻炼中", "外卖用餐中"};
        return names[w->action];
    }
    return "闲逛中";
}
const char *pw_message_text(const pet_world_t *w)
{
    if (w->now >= w->message_until)
        return "";
    if (w->action == PW_MEMORY && w->challenge.kind != PG_NONE) return pw_state_text(w);
    if(w->message==MSG_STUDY && w->activity_kind==1)return "安静自习中";
    if(w->message==MSG_EAT && w->activity_kind==2)return "吃份暖暖的简餐";
    static const char *texts[] = {"",
                                  "走过去啦",
                                  "沿路走到门口",
                                  "开饭啦",
                                  "呼噜呼噜",
                                  "认真听课中",
                                  "今天也要写代码",
                                  "晚安喵",
                                  "睡醒啦",
                                  "完成啦",
                                  "等当前动作结束",
                                  "先点床叫醒小猫",
                                  "稍等一下再来",
                                  "已经很满足啦",
                                  "肚子饿了，先去食堂",
                                  "请点对应的家具",
                                  "跑步机锻炼中",
                                  "下单成功，外卖配送中",
                                  "外卖到了，点门旁纸袋",
                                  "取到外卖啦",
                                  "已有一份外卖",
                                  "还没有可取的外卖",
                                  "看一遍，再按顺序点",
                                  "收下一枚校园贴纸",
                                  "金币不够，玩小游戏赚生活费",
                                  "买好啦，已摆进宿舍",
                                  "已摆进宿舍",
                                  "已收起，随时可以再摆",
                                  "今天的饭钱到账啦",
                                  "今天已经领过饭钱啦",
                                  "先连接手机校准时间",
                                  "签到日期在未来，请校准时间",
                                  "金币已满，消费后再领",
                                  "宵禁中，早上6点再出门"};
    return w->message >= 0 && w->message < (int)(sizeof(texts) / sizeof(texts[0])) ? texts[w->message] : "";
}
const char *pw_life_text(pw_scene_t scene)
{
    static const char *names[] = {"照看盆栽", "整理书架", "餐后小歇", "", "放松伸展"};
    return (unsigned)scene <= PW_GYM ? names[scene] : "";
}
const char *pw_need_text(const pet_world_t *w)
{
    if (w->phase == PW_SLEEP) return "正在休息，点床可以叫醒";
    if (pw_busy(w)) return pw_state_text(w);
    if (w->food < 20 && w->wallet.coins < pe_price(PE_MEAL)) return "先玩小游戏赚生活费，再买饭";
    if (w->food < 20) return w->scene == PW_CANTEEN ? "肚子饿啦，点窗口选餐" : "肚子饿啦，去食堂吃饭吧";
    if (w->clock.valid && pt_bedtime(w->clock.utc))
        return pt_bedtime(w->clock.utc) == 2 ? "有点困，回床上休息吧" : "准备睡觉啦 · 23点前休息";
    if (w->order == PW_ORDER_READY) return "外卖到了，回宿舍取餐";
    if (w->mood < 30) return "想和你贴贴，也想玩一会";
    if (w->wallet.coins < pe_price(PE_SNACK)) return "小游戏通关可以赚生活费";
    return "今天状态不错，随心逛逛";
}
