#include "pet_audio.h"
#include <assert.h>
#include <stdio.h>
static void report(const char *name, bool ok)
{
    printf("[host:audio] %s %s\n", name, ok ? "PASS" : "FAIL");
}
int main(void)
{
    assert(pa_selftest(report));
    puts("[host:audio] pure PCM and event policy PASS; no physical sound verdict");
    return 0;
}
