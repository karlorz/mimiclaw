#pragma once

#include <stddef.h>
#include <stdlib.h>

#define MALLOC_CAP_SPIRAM   0x1
#define MALLOC_CAP_INTERNAL 0x2

static inline void *heap_caps_calloc(size_t n, size_t size, int caps)
{
    (void)caps;
    return calloc(n, size);
}

static inline void *heap_caps_realloc(void *ptr, size_t size, int caps)
{
    (void)caps;
    return realloc(ptr, size);
}

static inline size_t heap_caps_get_free_size(int caps)
{
    (void)caps;
    return 0;
}
