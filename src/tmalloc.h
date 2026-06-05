#pragma once

#include <stddef.h>
#include <stdint.h>
#include <string.h>

typedef struct {
    unsigned char *top;
    unsigned char *end;
} tmalloc_t;

/* initialize arena */
void t_init(tmalloc_t *pool, void *start, size_t size);

/* aligned bump allocation */
void* t_malloc(tmalloc_t *pool, size_t size, size_t alignment);

/* convenience macro */
#define TNEW(pool, type) \
    (t_malloc((pool), sizeof(type), _Alignof(type)))