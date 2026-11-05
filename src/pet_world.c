/* Fixed-size navigation and time-driven rules. No LVGL or RTOS dependency. */
#include "pet_world.h"
#include <float.h>
#include <math.h>
#include <string.h>

static const pw_point_t nodes[4][32] = {
    {{365, 300}, {339, 305}, {301, 342}, {211, 339}, {161, 346}, {198, 390}, {120, 268}, {303, 277}},
    {{368, 231}, {338, 267}, {295, 319}, {224, 338}, {186, 369}, {244, 410}, {154, 383}, {0, 0}},
    {{365, 266}, {335, 290}, {288, 327}, {217, 349}, {183, 380}, {242, 411}, {125, 386}, {0, 0}},
    {{560, 560},
     {360, 560},
     {160, 560},
     {560, 360},
     {560, 160},
     {760, 560},
     {960, 560},
     {560, 760},
     {560, 960},
     {160, 160},
     {960, 160},
     {960, 960},
     {160, 960},
     {360, 460},
     {560, 120},
     {760, 460}}};
static const uint8_t campus_edges[][2] = {{0, 1},  {1, 2},  {0, 3},  {3, 4},  {0, 5},  {5, 6},  {0, 7},
                                          {7, 8},  {2, 9},  {9, 4},  {4, 10}, {10, 6}, {6, 11}, {11, 8},
                                          {8, 12}, {12, 2}, {1, 13}, {4, 14}, {5, 15}};
static const uint8_t edges[9][2] = {{0, 1}, {1, 2}, {2, 3}, {3, 4}, {2, 5}, {3, 5}, {4, 6}, {1, 7}, {2, 7}};
static const uint32_t durations[7] = {0, 4000, 1500, 600, 8000, 10000, 500};
static const uint32_t cooldowns[7] = {0, 2000, 3000, 0, 8000, 10000, 0};
static const int food_delta[7] = {0, 15, 0, 0, -5, -3, 0}, mood_delta[7] = {0, 0, 10, 0, 5, 4, 0};
static const pw_point_t supports[3][2] = {{{62, 195}, {293, 210}}, {{108, 310}, {0, 0}}, {{79, 336}, {0, 0}}};
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
    MSG_INVALID
};
static float distance(float ax, float ay, float bx, float by)
{
    return hypotf(ax - bx, ay - by);
}
pw_point_t pw_node(pw_scene_t s, unsigned n)
{
    return (unsigned)s <= PW_CAMPUS && n < 32 ? nodes[s][n] : (pw_point_t){0, 0};
}
uint32_t pw_duration(pw_command_t a)
{
    return a >= PW_EAT && a <= PW_WAKE ? durations[a] : 0;
}
bool pw_busy(const pet_world_t *w)
{
    return w->phase == PW_ACTION || w->phase == PW_SLEEP || w->phase == PW_TRANSITION;
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
    w->random = w->random * 1664525U + 1013904223U;
    w->idle_until = w->now + 2500 + (w->random % 2501U);
}
void pw_init(pet_world_t *w)
{
    memset(w, 0, sizeof(*w));
    w->scene = PW_DORM;
    w->phase = PW_IDLE;
    w->food = 70;
    w->mood = 60;
    w->node = 3;
    w->edge_a = w->edge_b = -1;
    w->destination = -1;
    w->point_edge_a = w->point_edge_b = -1;
    w->x = nodes[0][3].x;
    w->y = nodes[0][3].y;
    w->facing = 1;
    w->random = 0xC0FAU;
    w->idle_until = 3200;
}
static void clock_step(pet_world_t *w, uint64_t dt)
{
    w->food_elapsed += dt;
    uint64_t n = w->food_elapsed / 300000U;
    w->food_elapsed %= 300000U;
    w->food = n >= w->food ? 0 : (uint8_t)(w->food - n);
    if (w->phase == PW_SLEEP)
    {
        w->rest_elapsed += dt;
        n = w->rest_elapsed / 60000U;
        w->rest_elapsed %= 60000U;
        w->mood = n >= 100U - w->mood ? 100 : (uint8_t)(w->mood + n);
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
    if (a < PW_EAT || a > PW_REST + 2)
        return PW_INVALID;
    if (w->phase == PW_SLEEP)
        return PW_ASLEEP;
    if (pw_busy(w))
        return PW_BUSY;
    if (w->scene == PW_CAMPUS && a != PW_TOUCH)
        return PW_WRONG_ROOM;
    if (a == PW_EAT && w->scene != PW_CANTEEN)
        return PW_WRONG_ROOM;
    if (a == PW_STUDY && w->scene != PW_CLASSROOM)
        return PW_WRONG_ROOM;
    if ((a == PW_CODE || a == PW_REST) && w->scene != PW_DORM)
        return PW_WRONG_ROOM;
    if (w->cooldown[a] > w->now)
        return PW_COOLDOWN;
    if ((a == PW_EAT && w->food == 100) || (a == PW_TOUCH && w->mood == 100))
        return PW_FULL;
    if ((a == PW_STUDY && w->food < 10) || (a == PW_CODE && w->food < 8))
        return PW_LOW_FOOD;
    return PW_OK;
}
static pw_result_t reject(pet_world_t *w, pw_result_t r)
{
    static const int msgs[] = {MSG_NONE, MSG_BUSY, MSG_ASLEEP,  MSG_COOLDOWN,
                               MSG_FULL, MSG_LOW,  MSG_INVALID, MSG_INVALID};
    say(w, msgs[r]);
    return r;
}
static void begin(pet_world_t *w, pw_command_t a)
{
    w->phase = PW_ACTION;
    w->action = a;
    w->target = PW_NONE;
    w->purpose = 0;
    w->route_len = w->route_at = 0;
    w->action_start = w->now;
    w->action_end = w->now + durations[a];
    uint8_t old = w->food;
    w->food = clamp_value(w->food + food_delta[a]);
    w->mood = clamp_value(w->mood + mood_delta[a]);
    if (a != PW_REST && a != PW_WAKE)
        w->settlements[a]++;
    w->gain = w->food - old;
    say(w, a == PW_EAT     ? MSG_EAT
           : a == PW_TOUCH ? MSG_TOUCH
           : a == PW_STUDY ? MSG_STUDY
           : a == PW_CODE  ? MSG_CODE
           : a == PW_REST  ? MSG_REST
                           : MSG_WAKE);
}
static pw_point_t point(const pet_world_t *w, int n)
{
    return w->scene == PW_CAMPUS && n == 16 ? w->point_goal : pw_node(w->scene, (unsigned)n);
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
    *x = w->scene == PW_CAMPUS ? fmaxf(0, fminf(730, w->x - 195)) : 0;
    *y = w->scene == PW_CAMPUS ? fmaxf(-180, fminf(670, w->y - 320)) : 0;
}
static bool shortest(const pet_world_t *w, int start, int goal, int8_t *route, unsigned *len, float *cost)
{
    pw_scene_t s = w->scene;
    float dist[32];
    int8_t prev[32];
    bool done[32] = {false};
    int count = s == PW_CAMPUS ? 17 : (s == PW_DORM ? 8 : 7);
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
        for (int e = 0; e < (s == PW_CAMPUS ? (int)pw_road_count() + 2 : (s == PW_DORM ? 9 : 7)); e++)
        {
            int a, b;
            if (s != PW_CAMPUS)
            {
                a = edges[e][0];
                b = edges[e][1];
            }
            else if (e < (int)pw_road_count())
            {
                a = campus_edges[e][0];
                b = campus_edges[e][1];
            }
            else
            {
                a = 16;
                b = e == (int)pw_road_count() ? w->point_edge_a : w->point_edge_b;
            }
            if (a < 0 || b < 0)
                continue;
            int next = a == at ? b : (b == at ? a : -1);
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
    int8_t rev[32];
    unsigned n = 0;
    for (int x = goal; x >= 0; x = prev[x])
    {
        rev[n++] = (int8_t)x;
        if (x == start)
            break;
        if (n == 32)
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
    w->purpose = 0;
    w->edge_a = w->edge_b = -1;
    if (purpose == 1)
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
        w->phase = PW_TRANSITION;
        w->transition_start = w->now;
        w->swapped = false;
    }
    else
    {
        idle(w);
        if (w->scene == PW_CAMPUS && w->node == 16)
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
    if (w->scene == PW_CAMPUS && (w->edge_a == 16 || w->edge_b == 16))
    {
        w->edge_a = w->point_edge_a;
        w->edge_b = w->point_edge_b;
    }
    int anchor[2] = {w->edge_a >= 0 ? w->edge_a : w->node, w->edge_b};
    int8_t best[32];
    unsigned best_n = 0;
    float best_cost = FLT_MAX;
    for (int k = 0; k < 2; k++)
        if (anchor[k] >= 0)
        {
            int8_t route[32];
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
    if (w->scene != PW_CAMPUS || !isfinite(x) || !isfinite(y) || x < 0 || y < 0 || x > 1120 || y > 1120)
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
    if (best > 40 || edge < 0)
        return PW_INVALID;
    if (w->edge_a == 16 || w->edge_b == 16)
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
        w->route[0] = 16;
        w->route_at = 0;
        w->route_len = 1;
        w->phase = PW_WALK;
        w->action = PW_NONE;
        w->purpose = 3;
        consume(w);
        return PW_OK;
    }
    return move(w, 16, 3) ? PW_OK : PW_INVALID;
}
pw_result_t pw_request(pet_world_t *w, pw_command_t cmd)
{
    if (cmd == PW_EXIT || (cmd >= PW_ENTER_DORM && cmd <= PW_ENTER_CANTEEN))
    {
        if (pw_busy(w))
            return reject(w, w->phase == PW_SLEEP ? PW_ASLEEP : PW_BUSY);
        if ((cmd == PW_EXIT) == (w->scene == PW_CAMPUS))
            return PW_WRONG_ROOM;
        int next = cmd == PW_EXIT ? PW_CAMPUS : cmd - PW_ENTER_DORM;
        int goal = cmd == PW_EXIT ? 0 : 13 + next;
        w->entry_node = cmd == PW_EXIT ? (int8_t)(13 + w->scene) : 0;
        w->destination = (int8_t)next;
        w->target = PW_NONE;
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
        w->destination = (int8_t)next;
        w->target = PW_NONE;
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
    if (w->phase == PW_WALK && w->purpose == 1 && w->target == cmd)
        return PW_OK;
    w->destination = -1;
    if (cmd == PW_TOUCH)
    {
        w->route_len = w->route_at = 0;
        begin(w, cmd);
        return PW_OK;
    }
    w->target = cmd;
    if (!move(w, cmd == PW_CODE ? 7 : 6, 1))
        return reject(w, PW_INVALID);
    say(w, MSG_WALK);
    return PW_OK;
}
static void finish(pet_world_t *w)
{
    pw_command_t a = w->action;
    w->action = PW_NONE;
    if (a == PW_REST)
    {
        w->phase = PW_SLEEP;
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
        else if (w->phase == PW_ACTION)
            until = w->action_end - w->now;
        else if (w->phase == PW_TRANSITION)
            until = w->transition_start + (w->swapped ? 400 : 200) - w->now;
        else if (w->phase == PW_IDLE)
            until = w->idle_until > w->now ? w->idle_until - w->now : 1;
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
        else if (w->phase == PW_ACTION && w->now >= w->action_end)
            finish(w);
        else if (w->phase == PW_TRANSITION)
        {
            if (!w->swapped && w->now >= w->transition_start + 200)
            {
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
        else if (w->phase == PW_IDLE && w->now >= w->idle_until)
        {
            if (w->scene == PW_CAMPUS)
            {
                w->idle_until = w->now + 60000;
                continue;
            }
            int choices[4], count = 0;
            for (int n = 2; n <= 5; n++)
                if (distance(w->x, w->y, point(w, n).x, point(w, n).y) > 12)
                    choices[count++] = n;
            w->random = w->random * 1664525U + 1013904223U;
            if (count)
                move(w, choices[w->random % (unsigned)count], 0);
            else
                idle(w);
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
        v->scale = .82f;
    bool furniture = w->phase == PW_SLEEP || (w->phase == PW_ACTION && w->action != PW_TOUCH);
    bool settled = false;
    if (furniture)
    {
        bool bed = w->phase == PW_SLEEP || w->action == PW_REST || w->action == PW_WAKE;
        pw_point_t target = supports[w->scene][!bed && w->scene == PW_DORM ? 1 : 0];
        float progress = 1, amount = 1;
        int mode = 0;
        uint64_t elapsed = w->now - w->action_start;
        uint32_t enter = bed ? 600 : 500;
        if (w->phase == PW_ACTION)
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
            else if (w->action != PW_REST && elapsed > durations[w->action] - 500)
            {
                mode = 2;
                progress = (float)(elapsed - durations[w->action] + 500) / 500;
            }
        }
        float ease = progress * progress * (3 - 2 * progress);
        amount = mode == 2 ? 1 - ease : (mode == 1 ? ease : 1);
        v->lift = mode ? sinf(3.141592654f * progress) * (bed ? 23 : 17) : 0;
        v->x += (target.x - w->x) * amount;
        v->y += (target.y - w->y) * amount - v->lift;
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
    if (w->phase == PW_SLEEP)
        return "睡觉中";
    if (w->phase == PW_TRANSITION)
        return "切换场景";
    if (w->phase == PW_WALK)
        return w->purpose == 2 ? "前往门口" : (w->purpose == 1 ? "走向家具" : "散步中");
    if (w->phase == PW_ACTION)
    {
        static const char *names[] = {"闲逛中", "吃饭中", "摸摸", "准备睡觉", "上课中", "敲代码", "起床中"};
        return names[w->action];
    }
    return "闲逛中";
}
const char *pw_message_text(const pet_world_t *w)
{
    if (w->now >= w->message_until)
        return "";
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
                                  "请点对应的家具"};
    return w->message >= 0 && w->message < (int)(sizeof(texts) / sizeof(texts[0])) ? texts[w->message] : "";
}
