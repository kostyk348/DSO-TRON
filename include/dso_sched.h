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
    uint32_t        used_mask;  // bit i = ready_ids[i] уже выполнен в этом тике
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
        if (sl->period == 0) continue;        // period 0 = slot disabled; tick % 0 — аппаратное деление на ноль
        if (n >= sched->num_slots) break;     // в ready_ids не больше num_slots элементов
        if (tick % sl->period == sl->offset) {
            ids[n++] = sl->task_id;
        }
    }

    sched->exec_index = 0;
    sched->num_ready  = n;
    sched->used_mask  = 0;
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

    /*
     * Выбор следующей задачи — по priority слота (меньше = раньше), при равенстве
     * сохраняется порядок слотов. Сложность O(num_ready * num_slots) <= 16*16 при
     * DSO_MAX_SLOTS=16 и выполняется только на реально готовых задачах; маска used_mask
     * избавляет от лишнего массива, чтобы ничего не выделять динамически.
     */
    int      best_ready = -1;
    uint32_t best_slot  = 0;
    uint32_t best_prio  = 0xFFFFFFFFu;

    for (uint32_t i = 0; i < sched->num_ready; i++) {
        if (sched->used_mask & (1u << i)) continue;
        uint8_t id = sched->ready_ids[i];
        for (uint32_t s = 0; s < sched->num_slots; s++) {
            if (sched->slots[s].task_id != id) continue;
            if (sched->slots[s].priority < best_prio) {
                best_prio  = sched->slots[s].priority;
                best_slot  = s;
                best_ready = (int)i;
            }
            break; // один task_id — один слот
        }
    }

    if (best_ready < 0) return -1;

    sched->used_mask |= (1u << (uint32_t)best_ready);
    sched->exec_index++;
    sched->slots[best_slot].handler();
    return 0;
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
    sched->used_mask  = 0;
    sched->tick       = 0;
}

#endif
