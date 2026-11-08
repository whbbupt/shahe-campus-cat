#include "pet_economy.h"
const pe_food_t *pe_menu(bool delivery, unsigned index)
{
    static const pe_food_t meals[2][PE_MENU_COUNT] = {
        {{"番茄鸡蛋饭",6,20,0}, {"香菇鸡肉饭",7,24,1}, {"土豆牛肉饭",9,30,2},
         {"时蔬豆腐饭",5,16,1}, {"热汤面",6,20,1}, {"红豆小点心",3,10,1}},
        {{"照烧鸡腿饭",8,12,1}, {"咖喱鸡肉饭",9,16,1}, {"番茄牛腩饭",11,22,2},
         {"香菇素食饭",7,12,1}, {"鲜蔬拌面",8,14,1}, {"暖胃小米粥",5,8,1}}
    };
    return index < PE_MENU_COUNT ? &meals[delivery ? 1 : 0][index] : 0;
}
const pe_food_t *pe_order(unsigned code)
{
    static const pe_food_t legacy = {"暖饭便当",8,12,0};
    return code == 0 ? &legacy : pe_menu(true, code - 1);
}
unsigned pe_price(pe_item_t item)
{
    static const unsigned prices[] = {6, 3, 8, 40, 60, 80};
    return (unsigned)item < PE_ITEM_COUNT ? prices[item] : 0;
}
const char *pe_name(pe_item_t item)
{
    static const char *names[] = {"食堂套餐", "暖心点心", "宿舍外卖", "窗台盆栽", "校园挂画", "桌面书架"};
    return (unsigned)item < PE_ITEM_COUNT ? names[item] : "";
}
bool pe_valid(const pe_wallet_t *wallet)
{
    return wallet && wallet->coins <= PE_MAX_COINS && !(wallet->owned & ~PE_FURNITURE_MASK) &&
        !(wallet->equipped & ~wallet->owned);
}
bool pe_pay(pe_wallet_t *wallet, unsigned amount)
{
    if (!pe_valid(wallet) || amount > wallet->coins) return false;
    wallet->coins = (uint16_t)(wallet->coins - amount);
    return true;
}
unsigned pe_credit(pe_wallet_t *wallet, unsigned amount)
{
    if (!pe_valid(wallet)) return 0;
    unsigned credit = PE_MAX_COINS - wallet->coins;
    if (credit > amount) credit = amount;
    wallet->coins = (uint16_t)(wallet->coins + credit);
    return credit;
}
pe_result_t pe_furniture(pe_wallet_t *wallet, pe_item_t item)
{
    if (!pe_valid(wallet) || item < PE_PLANT || item > PE_SHELF) return PE_INVALID;
    unsigned bit = 1U << (item - PE_PLANT);
    if (wallet->owned & bit)
    {
        wallet->equipped ^= (uint8_t)bit;
        return wallet->equipped & bit ? PE_PLACED : PE_STORED;
    }
    if (!pe_pay(wallet, pe_price(item))) return PE_POOR;
    wallet->owned |= (uint8_t)bit;
    wallet->equipped |= (uint8_t)bit;
    return PE_BOUGHT;
}
