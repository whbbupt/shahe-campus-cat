#ifndef PET_ECONOMY_H
#define PET_ECONOMY_H
#include <stdbool.h>
#include <stdint.h>
enum { PE_START_COINS = 30, PE_MAX_COINS = 9999, PE_FURNITURE_MASK = 7 };
typedef enum { PE_MEAL, PE_SNACK, PE_DELIVERY, PE_PLANT, PE_PICTURE, PE_SHELF, PE_ITEM_COUNT } pe_item_t;
typedef struct { uint16_t coins; uint8_t owned, equipped; } pe_wallet_t;
typedef enum { PE_BOUGHT, PE_PLACED, PE_STORED, PE_POOR, PE_INVALID } pe_result_t;
enum { PE_MENU_COUNT = 6 };
typedef struct { const char *name; uint8_t price, food, mood; } pe_food_t;
const pe_food_t *pe_menu(bool delivery, unsigned index);
/* Code zero is a legacy/default order; explicit delivery choices use 1..6. */
const pe_food_t *pe_order(unsigned code);
unsigned pe_price(pe_item_t item);
const char *pe_name(pe_item_t item);
bool pe_valid(const pe_wallet_t *wallet);
bool pe_pay(pe_wallet_t *wallet, unsigned amount);
unsigned pe_credit(pe_wallet_t *wallet, unsigned amount);
pe_result_t pe_furniture(pe_wallet_t *wallet, pe_item_t item);
bool pe_selftest(void (*report)(const char *, bool));
#endif
