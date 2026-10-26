#ifndef PET_UI_H
#define PET_UI_H
#include "pet_world.h"
typedef pw_result_t (*pet_ui_action_fn)(pw_command_t command);
void pet_ui_create(pet_ui_action_fn action);
void pet_ui_update(const pet_world_t *world);
#endif
