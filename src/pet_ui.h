#ifndef PET_UI_H
#define PET_UI_H

#include "pet_model.h"
typedef void (*pet_ui_action_fn)(pet_event_t event);
/* UI-thread only. Recreating the page deletes its old animation timer first. */
void pet_ui_create(pet_ui_action_fn action);
void pet_ui_update(const pet_model_t *pet, const char *message);

#endif
