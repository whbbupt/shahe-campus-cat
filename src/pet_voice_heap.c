/* Dedicated PSRAM pool for voice codecs and TLS, as in the SDK Xiaozhi demo. */
#include "rtthread.h"
#include "mem_section.h"
#define MBEDTLS_PLATFORM_MEMORY
#include "mbedtls/platform.h"

L2_RET_BSS_SECT_BEGIN(pet_voice_heap)
ALIGN(8)
static uint8_t pool[1024 * 1024] L2_RET_BSS_SECT(pet_voice_heap);
L2_RET_BSS_SECT_END
static struct rt_memheap heap;

void *opus_heap_malloc(uint32_t size)
{
    return rt_memheap_alloc(&heap, size);
}

void opus_heap_free(void *memory)
{
    if (memory) rt_memheap_free(memory);
}

static void *voice_calloc(size_t count, size_t size)
{
    if (count && size > SIZE_MAX / count) return NULL;
    return rt_memheap_calloc(&heap, count, size);
}

static int voice_heap_init(void)
{
    if (rt_memheap_init(&heap, "vc_heap", pool, sizeof(pool)) != RT_EOK) return -RT_ERROR;
    return mbedtls_platform_set_calloc_free(voice_calloc, opus_heap_free);
}
INIT_PREV_EXPORT(voice_heap_init);
