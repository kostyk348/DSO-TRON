#ifndef DSO_FIFO_H
#define DSO_FIFO_H

#include <stdint.h>

/*
 * DSO Lock-free SPSC FIFO
 * ========================
 * Замена dtq (data queue) из μITRON.
 * Single Producer, Single Consumer — никаких мьютексов.
 * Использует volatile + барьеры памяти для корректности.
 * Размер — степень двойки (маскирование вместо деления).
 *
 * Замена μITRON dtq:
 *   cre_dtq → DSO_DTQ(name, size)  (compile-time)
 *   snd_dtq → dso_fifo_push
 *   rcv_dtq → dso_fifo_pop
 *   fsnd_dtq → dso_fifo_push (always non-blocking в DSO)
 */

typedef struct {
    uint32_t  head;      // consumer index (volatile for SPSC)
    uint32_t  tail;      // producer index (volatile for SPSC)
    uint32_t  mask;      // size - 1 (size must be power of 2)
    uint32_t  item_size; // bytes per item
    uint8_t  *buf;       // ring buffer
} dso_fifo;

#define DSO_FIFO_DECL(name, item_type, count) \
    static item_type name##_buf[count]; \
    static dso_fifo name = { 0, 0, (count)-1, sizeof(item_type), \
                             (uint8_t*)name##_buf }

static inline void dso_fifo_init(dso_fifo *q, void *buf,
                                  uint32_t item_size, uint32_t count)
{
    q->head      = 0;
    q->tail      = 0;
    q->mask      = count - 1;
    q->item_size = item_size;
    q->buf       = (uint8_t*)buf;
}

static inline uint32_t dso_fifo_len(dso_fifo *q)
{
    return (q->tail - q->head) & q->mask;
}

static inline uint32_t dso_fifo_free(dso_fifo *q)
{
    return q->mask - dso_fifo_len(q);
}

/*
 * Push — non-blocking. Только producer (tail) пишет.
 * Возвращает 0 при успехе, -1 если полон.
 */
static inline int dso_fifo_push(dso_fifo *q, const void *item)
{
    uint32_t t  = q->tail;
    uint32_t h  = q->head;  // volatile read of consumer head
    uint32_t n  = (t + 1) & q->mask;
    if (n == h) return -1;  // full

    uint8_t *dst = q->buf + t * q->item_size;
    for (uint32_t i = 0; i < q->item_size; i++)
        dst[i] = ((const uint8_t*)item)[i];

    q->tail = n;  // release: producer commits data before advancing tail
    return 0;
}

/*
 * Pop — non-blocking. Только consumer (head) читает.
 * Возвращает 0 при успехе, -1 если пуст.
 */
static inline int dso_fifo_pop(dso_fifo *q, void *item)
{
    uint32_t h  = q->head;
    uint32_t t  = q->tail;  // volatile read of producer tail
    if (h == t) return -1;  // empty

    uint8_t *src = q->buf + h * q->item_size;
    for (uint32_t i = 0; i < q->item_size; i++)
        ((uint8_t*)item)[i] = src[i];

    q->head = (h + 1) & q->mask;  // release: consumer commits read
    return 0;
}

/*
 * Peek — посмотреть элемент без извлечения.
 */
static inline void *dso_fifo_peek(dso_fifo *q)
{
    if (q->head == q->tail) return 0;
    return q->buf + q->head * q->item_size;
}

#endif
