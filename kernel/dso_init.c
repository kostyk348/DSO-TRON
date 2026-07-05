/*
 * dso_init.c — DSO-TRON Kernel Initialization
 * =============================================
 *
 * Замена _kernel_task_initialize и sysinit из URIBO.
 * Инициализирует DSO-подсистемы: арены, TT-планировщик, PIT.
 */

#include "kernel.h"
#include "../include/dso.h"
#include "../include/dso_config.h"

/* ── Global kernel tables (static, no malloc) ──────────── */

T_TCB *_kernel_tcb[DSO_MAX_TSK];
T_LNK  _kernel_rdq[DSO_MAX_TSKPRI + 1];

/* ── System memory (for sys_alloc fallback, used minimally) ── */

DSO_ARENA(_kernel_sys_arena, DSO_KERNEL_HEAP_SIZE);

void *_kernel_sys_alloc(SIZE size)
{
    return dso_arena_alloc(&_kernel_sys_arena, (uint32_t)size);
}

void _kernel_sys_free(void *p)
{
    (void)p;
    /* DSO: arena reset happens at boot. No free in TT. */
}

void *_kernel_stk_alloc(SIZE size)
{
    (void)size;
    /* DSO: stack comes from dso_tsk.c's _dso_stack_arena.
     * This is kept for compat but not used in DSO path. */
    return 0;
}

/* ── PIT-based Time Source ─────────────────────────────── */

static volatile uint32_t _dso_pit_ticks = 0;

/* Called from PIT IRQ handler (set in arch-specific code) */
void _dso_pit_isr(void)
{
    _dso_pit_ticks++;
    dso_tick_handler();
}

uint32_t dso_get_ticks(void)
{
    return _dso_pit_ticks;
}

/* ── Kernel bootstrap ──────────────────────────────────── */

void dso_kernel_init(void)
{
    /* Clear TCB table */
    for (int i = 0; i < DSO_MAX_TSK; i++)
        _kernel_tcb[i] = 0;

    /* Reset system arena */
    dso_arena_reset(&_kernel_sys_arena);

    /* Init ready queue (kept for minimal compat) */
    _kernel_nul_que(_kernel_rdq, DSO_MAX_TSKPRI);

    /* Init dummy wait queue */
    _kernel_dmy_que.p = &_kernel_dmy_que;
    _kernel_dmy_que.n = &_kernel_dmy_que;

    /* Kernel is ready for task management */
    _kernel_sts = T_KER_SYS;
}

/* ── System time ───────────────────────────────────────── */

ER get_tim(SYSTIM *pk_tim)
{
    if (!pk_tim) return E_PAR;
    /* DSO: return current system time from tick counter */
    pk_tim->ltime = _kernel_systim.ltime;
    pk_tim->utime = 0;
    return E_OK;
}
