/*
 * dso_common.c — DSO-TRON Kernel Core
 * ======================================
 *
 * Замена URIBO common.c.
 * Отличия от μITRON:
 *   - Нет динамической памяти (mpl/mpf вырезаны)
 *   - Нет приоритетной очереди (ready queue заменена на TT slot table)
 *   - Нет диспатча по приоритетам (_kernel_higher/_kernel_highest)
 *   - Есть только: активация по расписанию + lock-free IPC
 *
 * Сохраняет μITRON-совместимый API:
 *   sta_tsk, ext_tsk, slp_tsk, wup_tsk, snd_dtq, rcv_dtq
 */

#include "kernel.h"
#include "../include/dso.h"
#include "../include/dso_config.h"

/* ── Kernel globals ────────────────────────────────────── */

volatile T_TCB   *_kernel_cur = 0;     /* current task */
volatile UB       _kernel_sts = T_KER_SYS;
volatile UB       _kernel_dly = 0;
volatile TMO      _kernel_sysclk = 0;
volatile SYSTIM   _kernel_systim = {0, 0};
T_LNK             _kernel_dmy_que;     /* dummy queue for list init */

/* ── DSO Static Arena ──────────────────────────────────── */

DSO_POOL(_kernel_tcb_pool, T_TCB, DSO_MAX_TSK);

/* ── DSO TT Scheduler ──────────────────────────────────── */

static uint8_t        _dso_ready_scratch[DSO_MAX_TASKS_PER_TICK];
static dso_scheduler  _dso_sched;

const dso_slot       *_dso_slot_table = 0;
uint32_t              _dso_num_slots  = 0;

void dso_sched_register(const dso_slot *slots, uint32_t num)
{
    _dso_slot_table = slots;
    _dso_num_slots  = num;
    dso_sched_init(&_dso_sched, slots, num, _dso_ready_scratch);
}

/* ── TCB from Static Arena (replaces _kernel_sys_alloc) ── */

T_TCB *_kernel_tcb_alloc(void)
{
    return (T_TCB*)dso_pool_alloc(&_kernel_tcb_pool);
}

/* ── Queue management (kept from URIBO) ────────────────── */

void _kernel_nul_que(T_LNK *que, PRI pri)
{
    for (; pri--; que++)
        que->p = que->n = que;
}

void _kernel_enq(T_LNK *que, T_LNK *add)
{
    T_LNK *end;
    if (add->n) _KERNEL_PANIC();
    add->n = que;
    end = que->p;
    add->p = end;
    end->n = add;
    que->p = add;
}

void _kernel_deq(T_LNK *del)
{
    T_LNK *p = del->p;
    T_LNK *n = del->n;
    if (!n) _KERNEL_PANIC();
    p->n = n;
    n->p = p;
    del->n = NULL;
}

T_LNK *_kernel_first(T_LNK *que, PRI pri)
{
    for (; pri; pri--, que++) {
        if (que->n != que) return que->n;
    }
    return NULL;
}

/* ── Time-Triggered Dispatch (replaces _kernel_run/higher) ── */

void _kernel_run(T_TCB *new)
{
    /* 
     * DSO: нет приоритетов. Если диспатч разрешён — просто переключаем.
     * В TT-режиме диспатч происходит только по тику таймера.
     * Здесь мы просто помечаем задачу как готовую — PIT-обработчик
     * запустит её в своём слоте.
     */
    if (_KERNEL_CHK_ISLD(_kernel_sts)) {
        _kernel_dly = 1;
    } else {
        _KERNEL_SW(new);
    }
}

void _kernel_higher(T_TCB *new)
{
    /* DSO: нет higher-priority check. Просто диспатч. */
    if (_KERNEL_CHK_ISLD(_kernel_sts)) {
        _kernel_dly = 1;
    } else {
        _KERNEL_SW(new);
    }
}

void _kernel_highest(void)
{
    /* DSO: TT-планировщик решает, кто следующий. */
    if (_KERNEL_CHK_ISLD(_kernel_sts)) {
        _kernel_dly = 1;
    } else {
        /* Запускаем следующую задачу из TT-расписания */
        if (dso_sched_run_next(&_dso_sched) == 0) {
            /* got a task — handler was called */
        }
    }
}

/* ── Wait/Release mechanism (kept from URIBO, no timeout) ── */

ER _kernel_wai(T_TCB *tcb, TMO tmout, T_LNK *que, BOOL pri)
{
    (void)tmout; /* DSO: timeout не используется в TT режиме */
    _kernel_deq((T_LNK *)tcb);
    tcb->sts = TTS_WAI;
    /* DSO: timeout support опционален. Без таймерной очереди. */
    if (pri) que += (tcb->pri - 1);
    _kernel_enq(que, (T_LNK *)tcb);
    _kernel_highest();
    return _KERNEL_GET_TCB_RET(tcb);
}

void _kernel_deq_rdy_dsp(T_TCB *tcb)
{
    if (!(tcb->sts & TTS_WAI)) _KERNEL_PANIC();
    _kernel_deq((T_LNK *)tcb);
    if (tcb->sts == TTS_WAI) {
        tcb->sts = TTS_RDY;
        _kernel_higher(tcb);
    } else {
        tcb->sts = TTS_SUS;
    }
}

B _kernel_deq_rdy(T_TCB *tcb)
{
    if (!(tcb->sts & TTS_WAI)) _KERNEL_PANIC();
    _kernel_deq((T_LNK *)tcb);
    if (tcb->sts == TTS_WAI) {
        tcb->sts = TTS_RDY;
        return 1;
    } else {
        tcb->sts = TTS_SUS;
        return 0;
    }
}

void _kernel_dlt(T_LNK *que, PRI pri)
{
    for (; pri; pri--, que++) {
        T_TCB *tcb = (T_TCB *)que->n;
        while (tcb != (T_TCB *)que) {
            T_TCB *tcb_n = tcb->n;
            _KERNEL_SET_TCB_RET(tcb, E_DLT);
            _kernel_deq_rdy(tcb);
            tcb = tcb_n;
        }
    }
}

void _kernel_act(T_TCB *tcb, VP_INT arg)
{
    (void)arg; /* DSO: arg passed but not used in TT mode */
    _KERNEL_INI_CTX();
    tcb->pri = tcb->ctsk->itskpri;
    tcb->sts = TTS_RDY;
    tcb->wup = 0;
    tcb->sus = 0;
    if (tcb->tex) {
        tcb->tex->ptn  = 0;
        tcb->tex->stat = TTEX_DIS;
    }
    /* DSO: регистрируем в TT-расписании вместо ready queue */
    _kernel_run(tcb);
}

/* ── PIT Tick Handler (Time Source) ────────────────────── */

void dso_tick_handler(void)
{
    /*
     * Вызывается из PIT IRQ.
     * 1. Инкрементим системные часы
     * 2. TT-планировщик вычисляет, какие задачи готовы в этом тике
     * 3. Запускаем их последовательно
     */
    _kernel_sysclk++;
    _kernel_systim.ltime++;

    /* DSO TT: вычисляем задачи для этого тика */
    dso_sched_tick(&_dso_sched);

    /* Запускаем все готовые задачи */
    while (dso_sched_run_next(&_dso_sched) == 0)
        ;
}
