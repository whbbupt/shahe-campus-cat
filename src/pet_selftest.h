#ifndef PET_SELFTEST_H
#define PET_SELFTEST_H

#include <stdbool.h>
typedef void (*pet_test_report_fn)(const char *name, bool passed);
bool pet_selftest_run(pet_test_report_fn report);

#endif
