#include "pet_economy.h"
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
