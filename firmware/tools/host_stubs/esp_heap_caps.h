/* Host-check stub. */
#pragma once
#include <stddef.h>
#define MALLOC_CAP_DEFAULT 0x0004
#define MALLOC_CAP_INTERNAL 0x0800
#define MALLOC_CAP_8BIT 0x0004
#define MALLOC_CAP_DMA 0x0008
#define MALLOC_CAP_SPIRAM 0x0400
#ifdef __cplusplus
extern "C" {
#endif
size_t heap_caps_get_free_size(unsigned caps);
void* heap_caps_malloc(size_t size, unsigned caps);
#ifdef __cplusplus
}
#endif
