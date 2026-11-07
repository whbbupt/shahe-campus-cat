#ifndef PET_UI_H
#define PET_UI_H
#include "pet_world.h"
typedef pw_result_t (*pet_ui_action_fn)(pw_command_t command);
enum { PET_SAVE_PENDING, PET_SAVE_SAVED, PET_SAVE_UNAVAILABLE, PET_SAVE_FAILED };
void pet_ui_set_save_status(unsigned status);
void pet_ui_set_audio_available(bool available);
typedef pw_result_t (*pet_ui_point_fn)(float x, float y);
void pet_ui_set_point_handler(pet_ui_point_fn handler);
void pet_ui_create(pet_ui_action_fn action);
void pet_ui_update(const pet_world_t *world);
#endif
