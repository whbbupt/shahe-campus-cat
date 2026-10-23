#include "pet_selftest.h"
#include <stdio.h>

static void report(const char *name, bool passed)
{
    printf("[host:pet] %s %s\n", name, passed ? "PASS" : "FAIL");
}

int main(void)
{
    bool passed = pet_selftest_run(report);
    puts(passed ? "[host:pet] all PASS; board not tested" : "[host:pet] FAIL");
    return passed ? 0 : 1;
}
