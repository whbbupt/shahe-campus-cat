#!/bin/sh
# Host-only wrapper: LVGL 9.4 uses POSIX strnlen, while run_pet_tests passes -std=c11.
exec gcc -std=gnu11 -D_GNU_SOURCE "$@"
