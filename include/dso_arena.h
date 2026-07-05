#ifndef DSO_ARENA_H
#define DSO_ARENA_H

#include <stddef.h>
#include <stdint.h>

/*
 * DSO Arena — Static Memory Arena
 * ================================
 * Замена mpl/mpf (dynamic memory pools) из μITRON.
 * Весь объекты выделяются compile-time в фиксированных массивах.
 * Фрагментации нет, malloc нет, mpl/mpf нет.
 *
 * Usage:
 *   DSO_ARENA(my_arena, 1024);        // declare arena
 *   uint8_t *p = dso_arena_alloc(&my_arena, 32);
 *   dso_arena_reset(&my_arena);        // reset at schedule slot boundary
 */

typedef struct {
    uint8_t  *base;
    uint32_t  size;
    uint32_t  offset;
} dso_arena;

#define DSO_ARENA(name, sz) \
    static uint8_t name##_buf[sz]; \
    static dso_arena name = { name##_buf, sz, 0 }

static inline void dso_arena_init(dso_arena *a, void *buf, uint32_t size)
{
    a->base   = (uint8_t*)buf;
    a->size   = size;
    a->offset = 0;
}

static inline void *dso_arena_alloc(dso_arena *a, uint32_t size)
{
    uint32_t start = a->offset;
    uint32_t end   = start + size;
    if (end > a->size) return 0;
    a->offset = end;
    return a->base + start;
}

static inline void dso_arena_reset(dso_arena *a)
{
    a->offset = 0;
}

static inline uint32_t dso_arena_used(dso_arena *a)
{
    return a->offset;
}

/*
 * Statically-sized object pool (fixed-size blocks, no free)
 * Используется для TCB, DTQ и других kernel-объектов.
 */
typedef struct {
    void   *blocks;
    uint32_t block_size;
    uint32_t capacity;
    uint32_t count;
} dso_pool;

#define DSO_POOL(name, type, cnt) \
    static type name##_storage[cnt]; \
    static dso_pool name = { name##_storage, sizeof(type), cnt, 0 }

static inline void dso_pool_init(dso_pool *p, void *blocks,
                                  uint32_t block_size, uint32_t capacity)
{
    p->blocks     = blocks;
    p->block_size = block_size;
    p->capacity   = capacity;
    p->count      = 0;
}

static inline void *dso_pool_alloc(dso_pool *p)
{
    if (p->count >= p->capacity) return 0;
    return (uint8_t*)p->blocks + (p->count++) * p->block_size;
}

static inline uint32_t dso_pool_avail(dso_pool *p)
{
    return p->capacity - p->count;
}

/*
 * Resource Contract — compile-time budget for each task slot.
 */
typedef struct {
    uint32_t cycles_max;
    uint32_t stack_bytes;
    uint32_t deadline_us;
} dso_resource_contract;

#endif
