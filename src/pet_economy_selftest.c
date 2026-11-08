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
#define CHECK(c) do { if (!(c)) return false; } while (0)
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
    w.wallet.coins = 1000;
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
bool pw_economy_selftest(void (*report)(const char *, bool))
{
    bool a = earn_with_no_food(), b = result_boundaries(), c = purchases();
    if (report)
    {
        report("zero-coins-zero-food-can-earn-and-buy-meal", a);
        report("coin-once-cancel-cap-and-4999-5000-boundary", b);
        report("paid-food-arrival-cancel-and-owned-free-toggle", c);
    }
    return a && b && c;
}
