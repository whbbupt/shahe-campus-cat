#include "pet_save.h"
#include <stdio.h>
#include <assert.h>
#include <string.h>
static uint8_t bytes[PS_AREA_SIZE];static unsigned writes;
static bool rd(void *c,uint32_t o,uint8_t *p,size_t n){(void)c;memcpy(p,bytes+o,n);return true;}
static bool wr(void *c,uint32_t o,const uint8_t *p,size_t n){(void)c;(void)o;(void)p;(void)n;writes++;return true;}
static bool er(void *c,uint32_t o,size_t n){(void)c;(void)o;(void)n;writes++;return true;}
int main(void){
    FILE *f=fopen("campus-v10-nor.bin","rb");assert(f&&fread(bytes,sizeof(bytes),1,f)==1);fclose(f);
    pet_save_t s;pet_world_t w;pw_init(&w);ps_io_t io={NULL,rd,wr,er};
    assert(ps_open(&s,io,&w)==PS_VERSION&&!s.enabled&&writes==0);
    assert(ps_save(&s,&w)==PS_IO_ERROR&&writes==0);
    puts("[host:save-old6] actual a633ec9 schema6 refuses schema9, zero writes/erase PASS");return 0;
}
