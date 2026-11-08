#include "pet_save.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static uint8_t bytes[PS_AREA_SIZE];static unsigned writes;
static bool rd(void *c,uint32_t o,uint8_t *p,size_t n){(void)c;memcpy(p,bytes+o,n);return true;}
static bool wr(void *c,uint32_t o,const uint8_t *p,size_t n){(void)c;(void)o;(void)p;(void)n;writes++;return true;}
static bool er(void *c,uint32_t o,size_t n){(void)c;(void)o;(void)n;writes++;return true;}
int main(void)
{
    FILE *f=fopen("campus-v9-nor.bin","rb");assert(f&&fread(bytes,sizeof(bytes),1,f)==1);fclose(f);
    pet_world_t w;pw_init(&w);pet_save_t s;ps_io_t io={NULL,rd,wr,er};
    assert(ps_open(&s,io,&w)==PS_VERSION&&!s.enabled&&writes==0);
    assert(ps_save(&s,&w)==PS_IO_ERROR&&writes==0);
    puts("[host:save-old8] actual 92293c0 schema8 refuses schema9 with zero writes/erase PASS");return 0;
}
