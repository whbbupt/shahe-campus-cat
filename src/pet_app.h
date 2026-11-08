#ifndef PET_APP_H
#define PET_APP_H

#include "rtthread.h"
#include "pet_link.h"
rt_err_t pet_app_start(void);
void pet_app_poll(void);
/* Main-thread value snapshot; no transport, storage or UI access. */
void pet_app_get_phone_status(pet_phone_status_t *status);

#endif
