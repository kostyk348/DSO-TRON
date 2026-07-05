#ifndef DSO_FIFO_H
#define DSO_FIFO_H

#include <stdint.h>
#include <stdatomic.h>

/*
 * DSO Lock-free SPSC FIFO
 * ========================
 * Single Producer, Single Consumer — никаких мьютексов.
 * Использует C11 atomics для корректности на weak memory models (ARM, Xtensa).
 * На x86 (TSO) атомики компилируются в обычные load/store.
 *
 * Размер — степень двойки (маскирование вместо деления).
 *
 * Producer (ISR или задача) пишет tail, читает head.
 * Consumer (DSO TT задача) читает tail, пишет head.
 */

typedef struct {
    atomic_uint  head;      // consumer writes; producer reads
    atomic_uint  tail;      // producer writes; consumer reads
    uint32_t     mask;      // size - 1 (size must be power of 2)
    uint32_t     item_size; // bytes per item
    uint8_t     *buf;       // ring buffer
} dso_fifo;

#define DSO_FIFO_DECL(name, item_type, count) \
    static item_type name##_buf[count]; \
    static dso_fifo name = { 0, 0, (count)-1, sizeof(item_type), \
                             (uint8_t*)name##_buf }

static inline void dso_fifo_init(dso_fifo *q, void *buf,
                                  uint32_t item_size, uint32_t count)
{
    atomic_init(&q->head, 0);
    atomic_init(&q->tail, 0);
    q->mask      = count - 1;
    q->item_size = item_size;
    q->buf       = (uint8_t*)buf;
}

static inline uint32_t dso_fifo_len(dso_fifo *q)
{
    uint32_t t = atomic_load_explicit(&q->tail, memory_order_acquire);
    uint32_t h = atomic_load_explicit(&q->head, memory_order_acquire);
    return (t - h) & q->mask;
}

static inline uint32_t dso_fifo_free(dso_fifo *q)
{
    return q->mask - dso_fifo_len(q);
}

/*
 * Push — non-blocking. Только producer (tail) пишет.
 * Использует memory_order_release для tail (consumer увидит данные до tail).
 */
static inline int dso_fifo_push(dso_fifo *q, const void *item)
{
    uint32_t t = atomic_load_explicit(&q->tail, memory_order_relaxed);
    uint32_t h = atomic_load_explicit(&q->head, memory_order_acquire);
    uint32_t n = (t + 1) & q->mask;
    if (n == h) return -1;

    uint8_t *dst = q->buf + t * q->item_size;
    for (uint32_t i = 0; i < q->item_size; i++)
        dst[i] = ((const uint8_t*)item)[i];

    atomic_store_explicit(&q->tail, n, memory_order_release);
    return 0;
}

/*
 * Pop — non-blocking. Только consumer (head) читает.
 * Использует memory_order_release для head (producer увидит, что слот свободен).
 */
static inline int dso_fifo_pop(dso_fifo *q, void *item)
{
    uint32_t h = atomic_load_explicit(&q->head, memory_order_relaxed);
    uint32_t t = atomic_load_explicit(&q->tail, memory_order_acquire);
    if (h == t) return -1;

    uint8_t *src = q->buf + h * q->item_size;
    for (uint32_t i = 0; i < q->item_size; i++)
        ((uint8_t*)item)[i] = src[i];

    atomic_store_explicit(&q->head, (h + 1) & q->mask, memory_order_release);
    return 0;
}

/*
 * Peek — посмотреть элемент без извлечения.
 */
static inline void *dso_fifo_peek(dso_fifo *q)
{
    uint32_t h = atomic_load_explicit(&q->head, memory_order_acquire);
    if (h == atomic_load_explicit(&q->tail, memory_order_acquire)) return 0;
    return q->buf + h * q->item_size;
}

#endif
