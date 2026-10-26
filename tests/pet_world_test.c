#include "pet_world.h"
#include <stdio.h>
static void report(const char *name,bool passed){printf("[host:world] %s %s\n",name,passed?"PASS":"FAIL");}
int main(void){bool pass=pw_selftest(report);puts(pass?"[host:world] all PASS":"[host:world] all FAIL");return pass?0:1;}
