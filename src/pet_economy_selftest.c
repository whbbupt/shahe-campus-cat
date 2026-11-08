#include "pet_economy.h"
bool pe_selftest(void (*report)(const char *, bool))
{
    pe_wallet_t wallet = {30, 0, 0};
    bool pay = !pe_pay(&wallet, 31) && wallet.coins == 30 && pe_pay(&wallet, 30) && wallet.coins == 0;
    pay = pay && pe_credit(&wallet, 10000) == 9999 && pe_credit(&wallet, UINT32_MAX) == 0;
    wallet.coins = 39;
    bool furniture = pe_furniture(&wallet, PE_PLANT) == PE_POOR && !wallet.owned && wallet.coins == 39;
    wallet.coins = 40;
    furniture = furniture && pe_furniture(&wallet, PE_PLANT) == PE_BOUGHT && !wallet.coins && wallet.owned == 1 && wallet.equipped == 1;
    furniture = furniture && pe_furniture(&wallet, PE_PLANT) == PE_STORED && !wallet.coins && !wallet.equipped;
    furniture = furniture && pe_furniture(&wallet, PE_PLANT) == PE_PLACED && !wallet.coins && wallet.equipped == 1;
    wallet.equipped = 2;
    furniture = furniture && !pe_valid(&wallet) && pe_furniture(&wallet, PE_PLANT) == PE_INVALID;
    if (report) { report("wallet-bounds-and-no-overdraft", pay); report("furniture-own-once-free-toggle", furniture); }
    return pay && furniture;
}

#include "pet_world.h"
#if defined(_MSC_VER)
#include <stdio.h>
#define CHECK(c) do { if (!(c)) { fprintf(stderr,"economy selftest line %d: %s\n",__LINE__,#c); return false; } } while (0)
#else
#define CHECK(c) do { if (!(c)) return false; } while (0)
#endif
static void room(pet_world_t *w, pw_scene_t scene)
{
    pw_init(w); w->scene = scene;
    pw_point_t p = pw_node(scene, 3); w->x = p.x; w->y = p.y;
}
static bool arrival(pet_world_t *w)
{
    for (unsigned i = 0; i < 1000 && w->phase == PW_WALK; i++) pw_advance(w, 20);
    return w->phase != PW_WALK;
}
static bool earn_with_no_food(void)
{
    pet_world_t w; room(&w, PW_CANTEEN); w.food = 0; w.wallet.coins = 0;
    for (unsigned round = 0; round < 2; round++)
    {
        CHECK(pw_request(&w, PW_MEAL_GAME) == PW_OK && arrival(&w) && w.phase == PW_MINIGAME);
        pw_advance(&w, 5000);
        unsigned wrong = (w.challenge.targets[0] + 1) % 4;
        CHECK(pw_request(&w, (pw_command_t)(PW_MEMORY_0 + wrong)) == PW_OK);
        CHECK(w.wallet.coins == 2 * (round + 1) && w.coin_gain == 2 && !w.memory_wins);
        pw_request(&w, PW_MEMORY_0); CHECK(w.wallet.coins == 2 * (round + 1));
        CHECK(pw_request(&w, PW_MEMORY_CANCEL) == PW_OK);
        CHECK(pw_request(&w, PW_MEAL_GAME) == PW_COOLDOWN);
        pw_advance(&w, 60000);
    }
    CHECK(pw_request(&w, PW_BUY_SNACK) == PW_OK && w.food == 10 && w.wallet.coins == 1);
    CHECK(pw_request(&w, PW_EAT) == PW_NO_COINS && w.wallet.coins == 1);
    CHECK(pw_request(&w, PW_MEAL_GAME) == PW_OK && arrival(&w));
    for (unsigned i = 0; i < 3; i++)
        CHECK(pw_request(&w, (pw_command_t)(PW_MEMORY_0 + w.challenge.targets[i])) == PW_OK);
    CHECK(w.wallet.coins == 11 && w.memory_wins == 1 && w.coin_gain == 10);
    CHECK(pw_request(&w, PW_MEMORY_CANCEL) == PW_OK);
    CHECK(pw_request(&w, PW_EAT) == PW_OK && arrival(&w));
    CHECK(w.wallet.coins == 5 && w.food == 30 && w.settlements[PW_EAT] == 1);
    CHECK(pw_request(&w, PW_EAT) == PW_BUSY && w.wallet.coins == 5);
    CHECK(pw_request(&w, PW_BUY_SNACK) == PW_BUSY && w.wallet.coins == 5);
    return true;
}
static bool result_boundaries(void)
{
    for (unsigned elapsed = 4999; elapsed <= 5000; elapsed++)
    {
        pet_world_t w; room(&w, PW_CANTEEN);
        CHECK(pw_request(&w, PW_MEAL_GAME) == PW_OK && arrival(&w));
        CHECK(w.now - w.action_start <= elapsed);
        pw_advance(&w, elapsed - (uint32_t)(w.now - w.action_start));
        CHECK(w.now - w.action_start == elapsed);
        CHECK(pw_request(&w, (pw_command_t)(PW_MEMORY_0 + (w.challenge.targets[0] + 1) % 4)) == PW_OK);
        CHECK(w.wallet.coins == (elapsed == 5000 ? 32 : 30));
        pw_advance(&w, 1500); CHECK(w.phase == PW_IDLE);
    }
    pet_world_t w; room(&w, PW_CANTEEN); w.wallet.coins = 9997;
    CHECK(pw_request(&w, PW_MEAL_GAME) == PW_OK && arrival(&w));
    for (unsigned i = 0; i < 3; i++) pw_request(&w, (pw_command_t)(PW_MEMORY_0 + w.challenge.targets[i]));
    CHECK(w.wallet.coins == 9999 && w.coin_gain == 2);
    CHECK(pw_request(&w, PW_MEMORY_CANCEL) == PW_OK && w.wallet.coins == 9999);
    pw_advance(&w, 60000);
    CHECK(pw_request(&w, PW_MEAL_GAME) == PW_OK && arrival(&w));
    pw_advance(&w, 10000); CHECK(pw_request(&w, PW_MEMORY_CANCEL) == PW_OK && w.wallet.coins == 9999);
    return true;
}
static bool purchases(void)
{
    pet_world_t w; room(&w, PW_CANTEEN); w.wallet.coins = 6;
    CHECK(pw_request(&w, PW_EAT) == PW_OK && w.wallet.coins == 6);
    CHECK(pw_walk_to(&w, 275, 425) == PW_OK);
    pw_advance(&w, 20000); CHECK(w.wallet.coins == 6 && !w.settlements[PW_EAT]);
    CHECK(pw_request(&w, PW_EAT) == PW_OK);
    w.wallet.coins = 0;
    CHECK(arrival(&w) && w.phase == PW_IDLE && !w.settlements[PW_EAT]);
    room(&w, PW_DORM); w.wallet.coins = 1000;
    for (pw_command_t cmd = PW_BUY_PLANT; cmd <= PW_BUY_SHELF; cmd++) CHECK(pw_request(&w, cmd) == PW_OK);
    CHECK(w.wallet.coins == 820 && w.wallet.owned == 7 && w.wallet.equipped == 7);
    for (pw_command_t cmd = PW_BUY_PLANT; cmd <= PW_BUY_SHELF; cmd++)
    { CHECK(pw_request(&w, cmd) == PW_OK); CHECK(pw_request(&w, cmd) == PW_OK); }
    CHECK(w.wallet.coins == 820 && w.wallet.equipped == 7);
    w.food = 100; CHECK(pw_request(&w, PW_BUY_SNACK) == PW_FULL && w.wallet.coins == 820);
    w.phase = PW_SLEEP; CHECK(pw_request(&w, PW_BUY_PLANT) == PW_ASLEEP && w.wallet.equipped == 7);
    room(&w, PW_DORM); w.wallet.coins = 8;
    CHECK(pw_request(&w, PW_ORDER) == PW_OK && !w.wallet.coins);
    CHECK(pw_request(&w, PW_ORDER) == PW_ORDER_ACTIVE && !w.wallet.coins);
    pw_advance(&w, 30000);
    CHECK(pw_request(&w, PW_TAKEOUT) == PW_OK && arrival(&w));
    pw_advance(&w, 3200);
    CHECK(w.food == 82 && !w.wallet.coins && w.order == PW_ORDER_NONE);
    return true;
}
static bool daily_checkin(void)
{
    pet_world_t w; room(&w, PW_DORM); w.wallet.coins = 0;
    CHECK(pw_checkin_status(&w) == PW_CHECKIN_WAIT_TIME && pw_request(&w, PW_DAILY_CHECKIN) == PW_INVALID);
    pt_date_t date = {2026, 11, 8, 23, 59, 59}; uint32_t utc;
    CHECK(pt_encode(&date, &utc)); w.clock = (pt_clock_t){utc, true, false, PT_RTC, PT_OK};
    CHECK(pw_checkin_status(&w) == PW_CHECKIN_WAIT_TIME);
    w.clock.trusted = true;
    CHECK(pw_request(&w, PW_DAILY_CHECKIN) == PW_OK && w.wallet.coins == 6 && w.coin_gain == 6);
    unsigned today = w.checkin_day;
    CHECK(pw_request(&w, PW_DAILY_CHECKIN) == PW_COOLDOWN && w.wallet.coins == 6 && !w.cooldown[PW_MEMORY]);
    w.clock.utc++;
    CHECK(pw_request(&w, PW_DAILY_CHECKIN) == PW_OK && w.wallet.coins == 12 && w.checkin_day == today + 1);
    w.clock.utc -= 3600;
    CHECK(pw_checkin_status(&w) == PW_CHECKIN_FUTURE && pw_request(&w, PW_DAILY_CHECKIN) == PW_INVALID && w.wallet.coins == 12);
    w.clock.utc += 3600 + 7 * 86400;
    CHECK(pw_request(&w, PW_DAILY_CHECKIN) == PW_OK && w.wallet.coins == 18);
    w.clock.utc += 86400; w.wallet.coins = PE_MAX_COINS;
    unsigned last = w.checkin_day;
    CHECK(pw_request(&w, PW_DAILY_CHECKIN) == PW_FULL && w.checkin_day == last);
    w.wallet.coins = 9997;
    CHECK(pw_request(&w, PW_DAILY_CHECKIN) == PW_OK && w.wallet.coins == 9999 && w.coin_gain == 2);
    w.clock.utc += 86400; w.wallet.coins = 0; w.phase = PW_SLEEP;
    CHECK(pw_request(&w, PW_DAILY_CHECKIN) == PW_ASLEEP && !w.wallet.coins);
    w.phase = PW_WALK; CHECK(pw_request(&w, PW_DAILY_CHECKIN) == PW_BUSY && !w.wallet.coins);
    w.phase = PW_IDLE; w.clock.valid = false;
    CHECK(pw_checkin_status(&w) == PW_CHECKIN_WAIT_TIME);
    w.clock.valid = true;
    date = (pt_date_t){2024, 1, 1, 0, 0, 0}; CHECK(pt_encode(&date, &w.clock.utc)); w.checkin_day = 0;
    CHECK(pw_request(&w, PW_DAILY_CHECKIN) == PW_OK && w.checkin_day == PW_CHECKIN_MIN_DAY);
    date = (pt_date_t){2037, 12, 31, 23, 59, 59}; CHECK(pt_encode(&date, &w.clock.utc));
    CHECK(pw_request(&w, PW_DAILY_CHECKIN) == PW_OK && w.checkin_day == PW_CHECKIN_MAX_DAY);
    return true;
}
static bool scene_meals_and_balance(void)
{
    CHECK(!pe_menu(false, PE_MENU_COUNT) && !pe_order(PE_MENU_COUNT + 1));
    for (unsigned i = 0; i < PE_MENU_COUNT; i++)
    {
        pet_world_t w; room(&w, PW_CANTEEN); w.food=50; w.mood=50; w.wallet.coins=100;
        CHECK(pw_request(&w, PW_SIT_MEAL)==PW_OK && arrival(&w));
        CHECK(w.phase==PW_SEATED && w.wallet.coins==100 && w.food==50 && !w.settlements[PW_EAT]);
        CHECK(pw_request(&w, PW_MEAL_LEAVE)==PW_OK && w.phase==PW_IDLE && w.wallet.coins==100);
        CHECK(pw_request(&w, PW_SIT_MEAL)==PW_OK && arrival(&w));
        const pe_food_t *dish=pe_menu(false,i);
        CHECK(pw_request(&w,(pw_command_t)(PW_MEAL_FIRST+i))==PW_BUSY && w.wallet.coins==100);
        pw_advance(&w,500);
        CHECK(pw_request(&w,(pw_command_t)(PW_MEAL_FIRST+i))==PW_OK);
        CHECK(w.wallet.coins==100-dish->price && w.food==50+dish->food && w.mood==50+dish->mood);
        CHECK(w.action_end-w.action_start==pw_duration(PW_EAT));
        CHECK(pw_request(&w,(pw_command_t)(PW_MEAL_FIRST+(i+1)%PE_MENU_COUNT))==PW_BUSY);
        for(unsigned frame=0;frame<72;frame++)
        {
            pw_visual_t v;pw_visual(&w,&v);
            CHECK(v.x>=0 && v.x<390 && v.y>=0 && v.y<450 && v.lift>=-0.01f && v.lift<=17.01f);
            pw_advance(&w,50);
        }
        room(&w,PW_DORM);w.food=50;w.mood=50;w.wallet.coins=100;dish=pe_menu(true,i);
        CHECK(pw_request(&w,(pw_command_t)(PW_DELIVERY_FIRST+i))==PW_OK && w.order_choice==i+1 && w.wallet.coins==100-dish->price);
        CHECK(pw_request(&w,(pw_command_t)(PW_DELIVERY_FIRST+(i+1)%PE_MENU_COUNT))==PW_ORDER_ACTIVE && w.order_choice==i+1);
        pw_advance(&w,PW_DELIVERY_MS);
        CHECK(pw_request(&w,PW_TAKEOUT)==PW_OK && arrival(&w));pw_advance(&w,3200);
        CHECK(w.food==50+dish->food && w.mood==50+dish->mood && w.wallet.coins==100-dish->price && !w.order_choice);
    }
    pet_world_t w;room(&w,PW_DORM);w.food=100;
    CHECK(pw_request(&w,PW_DELIVERY_FIRST)==PW_FULL && w.wallet.coins==30 && !w.order_choice);
    w.food=70;w.wallet.coins=0;CHECK(pw_request(&w,PW_DELIVERY_FIRST)==PW_NO_COINS && !w.order_choice);
    room(&w,PW_DORM);CHECK(pw_request(&w,PW_TOUCH)==PW_OK && w.mood==63 && w.mood_gain==3);
    CHECK(pw_request(&w,PW_TOUCH)==PW_BUSY && w.mood==63);
    pw_advance(&w,1500 + PW_TOUCH_COOLDOWN_MS - 1);
    CHECK(pw_request(&w,PW_TOUCH)==PW_COOLDOWN && w.mood==63);
    pw_advance(&w,1);CHECK(pw_request(&w,PW_TOUCH)==PW_OK && w.mood==66);
    room(&w,PW_DORM);w.mood=99;CHECK(pw_request(&w,PW_TOUCH)==PW_OK && w.mood==100 && w.mood_gain==1);
    return true;
}
static void clock_at(pet_world_t *w,unsigned hour,unsigned minute,unsigned second)
{
    pt_date_t d={2026,11,9,hour,minute,second};pt_encode(&d,&w->clock.utc);w->clock.valid=w->clock.trusted=true;
}
static bool curfew_guards(void)
{
    const unsigned times[][4]={{22,59,59,0},{23,0,0,1},{5,59,59,1},{6,0,0,0}};
    for(unsigned i=0;i<4;i++)
    {
        pet_world_t w;room(&w,PW_DORM);clock_at(&w,times[i][0],times[i][1],times[i][2]);
        CHECK(pw_curfew(&w)==(times[i][3]!=0));
        CHECK(pw_request(&w,PW_EXIT)==(times[i][3]?PW_CURFEW:PW_OK));
        if(times[i][3])CHECK(w.scene==PW_DORM && w.phase==PW_IDLE && w.curfew_events==1);
    }
    pet_world_t w;room(&w,PW_DORM);clock_at(&w,23,0,0);
    CHECK(pw_request(&w,PW_GO_CLASSROOM)==PW_CURFEW && pw_request(&w,PW_GO_CANTEEN)==PW_CURFEW);
    CHECK(pw_request(&w,PW_TOUCH)==PW_OK);pw_advance(&w,2000);
    CHECK(pw_request(&w,PW_REST)==PW_OK && arrival(&w));pw_advance(&w,600);
    CHECK(w.phase==PW_SLEEP && pw_request(&w,PW_REST)==PW_OK);pw_advance(&w,600);CHECK(w.phase==PW_IDLE);
    CHECK(pw_request(&w,PW_DELIVERY_FIRST)==PW_OK);
    room(&w,PW_DORM);clock_at(&w,23,0,0);w.clock.trusted=false;CHECK(pw_request(&w,PW_EXIT)==PW_OK);
    room(&w,PW_DORM);clock_at(&w,22,59,59);CHECK(pw_request(&w,PW_EXIT)==PW_OK);clock_at(&w,23,0,0);pw_advance(&w,12000);
    CHECK(w.scene==PW_DORM && w.phase==PW_IDLE && w.curfew_events==1 && w.destination==-1);
    room(&w,PW_DORM);clock_at(&w,22,0,0);CHECK(pw_request(&w,PW_EXIT)==PW_OK);
    for(unsigned i=0;i<1000 && w.phase==PW_WALK;i++)pw_advance(&w,1);
    /* Place at door to exercise clock changes in the pre-swap transition. */
    pw_advance(&w,10000);
    room(&w,PW_DORM);pw_point_t door=pw_node(PW_DORM,0);w.x=door.x;w.y=door.y;w.node=0;
    clock_at(&w,22,0,0);CHECK(pw_request(&w,PW_EXIT)==PW_OK);CHECK(w.phase==PW_TRANSITION);
    clock_at(&w,23,0,0);pw_advance(&w,201);CHECK(w.scene==PW_DORM && w.phase==PW_IDLE && w.curfew_events==1);
    room(&w,PW_CAMPUS);door=pw_node(PW_CAMPUS,pw_campus_door(PW_DORM));w.x=door.x;w.y=door.y;w.node=(int8_t)pw_campus_door(PW_DORM);
    clock_at(&w,23,0,0);CHECK(pw_request(&w,PW_ENTER_DORM)==PW_OK);pw_advance(&w,5000);CHECK(w.scene==PW_DORM);
    return true;
}
bool pw_economy_selftest(void (*report)(const char *, bool))
{
    bool a = earn_with_no_food(), b = result_boundaries(), c = purchases(), d = daily_checkin(), e = scene_meals_and_balance(), f = curfew_guards();
    if (report)
    {
        report("zero-coins-zero-food-can-earn-and-buy-meal", a);
        report("coin-once-cancel-cap-and-4999-5000-boundary", b);
        report("paid-food-arrival-cancel-and-owned-free-toggle", c);
        report("daily-checkin-trusted-midnight-once-rollback-cap", d);
        report("twelve-meals-seated-cancel-once-and-touch-balance", e);
        report("curfew-boundaries-wake-old-route-crossing-and-return", f);
    }
    return a && b && c && d && e && f;
}
