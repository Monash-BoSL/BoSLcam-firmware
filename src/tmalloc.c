#include "tmalloc.h"

static uintptr_t align_up(uintptr_t p, size_t a) {
    return (p + (a - 1u)) & ~(uintptr_t)(a - 1u);
}

static size_t align_size(size_t n, size_t a) {
    return (n + (a - 1u)) & ~(a - 1u);
}

void t_init(tmalloc_t* pool, void* start, size_t size) {
    uintptr_t begin  = (uintptr_t)start;
    uintptr_t finish = begin + size;

    /* so that all objects from here are zero initialised */
    memset(begin, 0, size);

    /* default safe alignment for pool start */
    uintptr_t aligned = align_up(begin, 8u);

    if (aligned > finish) {
        pool->top = NULL;
        pool->end = NULL;
        return;
    }

    pool->top = (unsigned char *)aligned;
    pool->end = (unsigned char *)finish;
}

void* t_malloc(tmalloc_t* pool, size_t size, size_t alignment) {
    uintptr_t p;

    if (!pool || !pool->top || alignment == 0 || (alignment & (alignment - 1u))) {
        return NULL;
    }

    p = align_up((uintptr_t)pool->top, alignment);
    size = align_size(size, alignment);

    if (p + size > (uintptr_t)pool->end) {
        return NULL;
    }

    pool->top = (unsigned char *)(p + size);
    return (void *)p;
}