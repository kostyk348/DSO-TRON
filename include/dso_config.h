#ifndef DSO_CONFIG_H
#define DSO_CONFIG_H

/*
 * DSO-TRON System Configuration
 * ==============================
 * Все объекты статические — максимумы задаются compile-time.
 * Замена config.h из URIBO: никаких dynamic alloc, ID_ANY, acre_*.
 *
 * Настройки ниже — пример для x86_64 QEMU.
 * Меняются под конкретное железо.
 */

/* ── Task Configuration ────────────────────────────────── */
#define DSO_MAX_TSK       8      /* max tasks (static TCB_ARENA) */
#define DSO_MAX_TSKPRI    1      /* not used in TT, kept for compat */
#define DSO_STACK_SIZE    4096   /* per-task stack size */

/* ── IPC (Data Queue) Configuration ────────────────────── */
#define DSO_MAX_DTQ       8      /* max data queues */
#define DSO_DTQ_SIZE      16     /* elements per data queue */
#define DSO_DTQ_DATA_SIZE sizeof(void*) /* data queue element size */

/* ── Timer ─────────────────────────────────────────────── */
#define DSO_TICK_HZ       100    /* PIT tick frequency (Hz) */
#define DSO_TICK_US       (1000000 / DSO_TICK_HZ)  /* us per tick */

/* ── Scheduler ──────────────────────────────────────────── */
#define DSO_MAX_SLOTS     16     /* max TT schedule slots */
#define DSO_MAX_TASKS_PER_TICK 8 /* max tasks ready in one tick */

/* ── Memory ─────────────────────────────────────────────── */
#define DSO_KERNEL_HEAP_SIZE 8192  /* sys_alloc size */
#define DSO_USER_HEAP_SIZE   4096  /* user tasks arena */

/* ── Cut features (set 0 to disable) ────────────────────── */
#define DSO_USE_MUTEX     0      /* mutex: not needed in TT */
#define DSO_USE_SEMAPHORE 0      /* semaphore: replaced by TT */
#define DSO_USE_EVENTFLAG 0      /* eventflag: replaced by TT */
#define DSO_USE_MEMPOOL   0      /* mpl/mpf: replaced by static arena */
#define DSO_USE_ISR       0      /* ISR: PIT-only */
#define DSO_USE_RENDEZVOUS 0     /* rendezvous: cut */

#endif
