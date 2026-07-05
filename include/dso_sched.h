#ifndef DSO_SCHED_H
#define DSO_SCHED_H

#include <stdint.h>

/*
 * DSO Time-Triggered Scheduler
 * =============================
 * Замена приоритетного планировщика μITRON (_kernel_highest,
 * ready queue с битовыми картами).
 *
 * Статическое расписание — массив слотов, каждый с task_id,
 * периодом, смещением и дедлайном.
 * На каждом тике PIT: проходим по таблице, запускаем те задачи,
 * у которых (tick % period == offset).
 *
 * Нет готовой очереди, нет bitmap search, нет preemption —
 * чистый Time-Triggered.
 *
 * Usage:
 *   static const dso_slot schedule[] = {
 *       DSO_SLOT(1, 10, 0, 1000),   // task 1, every 10 ticks, offset 0
 *       DSO_SLOT(2, 10, 5, 500),    // task 2, every 10 ticks, offset 5
 *       DSO_SLOT(3, 50, 0, 2000),   // task 3, every 50 ticks
 *   };
 *   DSO_SCHEDULE(schedule);
 */

typedef void (*dso_task_fn)(void);

typedef struct {
    uint8_t      task_id;     // 0 = idle slot
    uint8_t      priority;    // within-tick execution order (lower = first)
    uint16_t     period;      // ticks between activations
    uint16_t     offset;      // first activation tick
    uint32_t     deadline;    // max cycles to complete
    dso_task_fn  handler;     // task entry point
    uint32_t     stack_bytes; // stack budget
} dso_slot;

#define DSO_SLOT(id, pri, period, off, dl, fn) \
    { (id), (pri), (period), (off), (dl), (fn), 0 }

#define DSO_SLOT_STACK(id, pri, period, off, dl, fn, stack) \
    { (id), (pri), (period), (off), (dl), (fn), (stack) }

/*
 * Scheduler state
 */
typedef struct {
    const dso_slot *slots;      // static schedule table
    uint32_t        num_slots;  // number of slots
    uint32_t        num_ready;  // number of ready tasks in current tick
    uint32_t        exec_index; // current executing slot index (0..num_ready)
    uint8_t        *ready_ids;  // array of task ids ready this tick (scratch)
    uint32_t        tick;       // current tick count
} dso_scheduler;

/*
 * Run one tick of the time-triggered scheduler.
 * Вызывается из PIT-обработчика.
 * Возвращает количество задач, запушенных в этом тике.
 */
static inline uint32_t dso_sched_tick(dso_scheduler *sched)
{
    uint32_t tick = sched->tick;
    uint32_t n    = 0;
    uint8_t *ids  = sched->ready_ids;

    for (uint32_t i = 0; i < sched->num_slots; i++) {
        const dso_slot *sl = &sched->slots[i];
        if (sl->task_id == 0) continue;       // idle
        if (tick % sl->period == sl->offset) {
            ids[n++] = sl->task_id;
        }
    }

    sched->exec_index = 0;
    sched->num_ready  = n;
    sched->tick       = tick + 1;
    return n;
}

/*
 * Execute next ready task. Return 0 if done, -1 if nothing ready.
 */
static inline int dso_sched_run_next(dso_scheduler *sched)
{
    if (sched->exec_index >= sched->num_ready)
        return -1;

    uint8_t id = sched->ready_ids[sched->exec_index];
    sched->exec_index++;

    // find slot by task_id and call handler
    for (uint32_t i = 0; i < sched->num_slots; i++) {
        if (sched->slots[i].task_id == id) {
            sched->slots[i].handler();
            return 0;
        }
    }
    return -1;
}

/*
 * Initialize scheduler with a static slot table.
 */
static inline void dso_sched_init(dso_scheduler *sched,
                                   const dso_slot *slots,
                                   uint32_t num_slots,
                                   uint8_t *ready_scratch)
{
    sched->slots      = slots;
    sched->num_slots  = num_slots;
    sched->num_ready  = 0;
    sched->exec_index = 0;
    sched->ready_ids  = ready_scratch;
    sched->tick       = 0;
}

#endif
