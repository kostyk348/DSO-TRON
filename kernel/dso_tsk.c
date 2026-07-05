/*
 * dso_tsk.c — DSO-TRON Task Management
 * ======================================
 *
 * Замена cre_tsk, del_tsk, sta_tsk, ext_tsk из URIBO.
 *
 * Отличия от μITRON:
 *   - TCB выделяется из Static Arena (нет malloc)
 *   - Стек задачи — часть статической арены (нет _kernel_stk_alloc)
 *   - Активация регистрирует задачу в TT-расписании
 *   - Нет ID_ANY (все ID фиксированы compile-time)
 *   - Нет TA_ACT (активация только через TT schedule)
 */

#include "kernel.h"
#include "../include/dso.h"
#include "../include/dso_config.h"

/* ── Stack Arena ───────────────────────────────────────── */
DSO_ARENA(_dso_stack_arena, DSO_MAX_TSK * DSO_STACK_SIZE);

/* ── cre_tsk — DSO Static Version ──────────────────────── */

ER cre_tsk(ID tskid, const T_CTSK *pk_ctsk)
{
    UINT psw;
    ER   r = E_OK;
    T_TCB *tcb;

#ifndef _KERNEL_NO_STATIC_ERR
    {
        UB s = _kernel_sts;
        if (_KERNEL_CHK_IL(s)) _KERNEL_RET(E_CTX);
        if (tskid == TSK_SELF)
            _KERNEL_RET(_KERNEL_CHK_IS(s) ? E_ID : E_OBJ);
        if (tskid < 1 || tskid > DSO_MAX_TSK)
            _KERNEL_RET(E_ID);
        if (!pk_ctsk || !pk_ctsk->task)
            _KERNEL_RET(E_PAR);
    }
#endif

    psw = _KERNEL_DIS_PSW();

    /* Allocate TCB from static pool (NO malloc) */
    tcb = _kernel_tcb_alloc();
    if (!tcb) _KERNEL_END(E_NOMEM);

    /* Allocate stack from static arena (NO malloc) */
    tcb->stk = (VP)(uintptr_t)dso_arena_alloc(&_dso_stack_arena, pk_ctsk->stksz);
    if (!tcb->stk) {
        _KERNEL_END(E_NOMEM);
    }

    /* Init TCB */
    tcb->id       = tskid;
    tcb->sts      = TTS_DMT;
    tcb->act      = 0;
    tcb->wup      = 0;
    tcb->sus      = 0;
    tcb->tex      = 0;
    tcb->pri      = pk_ctsk->itskpri;
    tcb->ctsk     = (T_CTSK*)pk_ctsk;
    tcb->tim.n    = NULL;

    _kernel_tcb[tskid - 1] = tcb;

end:
    _KERNEL_SET_PSW(psw);
    _KERNEL_ASSERT_CRE_TSK();
    return r;
}

/* ── acre_tsk stub (not supported in DSO) ─────────────── */
ER acre_tsk(const T_CTSK *pk_ctsk)
{
    (void)pk_ctsk;
    return E_NOSPT;
}

/* ── sta_tsk — Start task ──────────────────────────────── */

ER sta_tsk(ID tskid, VP_INT stacd)
{
    UINT   psw;
    ER     r = E_OK;
    T_TCB *tcb;

    (void)stacd;  /* DSO: start code not used */

    if (tskid < 1 || tskid > DSO_MAX_TSK) return E_ID;
    tcb = _kernel_tcb[tskid - 1];
    if (!tcb) return E_NOEXS;

    psw = _KERNEL_DIS_PSW();

    switch (tcb->sts) {
    case TTS_DMT:
        tcb->sts = TTS_RDY;
        _KERNEL_INI_CTX();
        break;
    case TTS_RDY:
    case TTS_RUN:
        tcb->act++;
        break;
    default:
        r = E_OBJ;
        goto end;
    }

end:
    _KERNEL_SET_PSW(psw);
    _KERNEL_ASSERT_STA_TSK();
    return r;
}

/* ── ext_tsk — Exit task ───────────────────────────────── */

ER ext_tsk(void)
{
    return exd_tsk();
}

ER exd_tsk(void)
{
    UINT   psw;
    T_TCB *tcb = (T_TCB*)_kernel_cur;

    psw = _KERNEL_DIS_PSW();
    tcb->sts = TTS_DMT;
    /* DSO: TT scheduler picks next task via highest() */
    _kernel_highest();
    _KERNEL_SET_PSW(psw);
    _KERNEL_ASSERT_EXD_TSK();
    return E_OK;
}

/* ── ter_tsk — Terminate task ──────────────────────────── */

ER ter_tsk(ID tskid)
{
    UINT   psw;
    ER     r = E_OK;
    T_TCB *tcb;

    if (tskid < 1 || tskid > DSO_MAX_TSK) return E_ID;
    tcb = _kernel_tcb[tskid - 1];
    if (!tcb) return E_NOEXS;

    psw = _KERNEL_DIS_PSW();

    if (tcb->sts == TTS_DMT) {
        r = E_OBJ;
        goto end;
    }

    tcb->sts = TTS_DMT;

    if (tcb == _kernel_cur) {
        _kernel_highest();
    }

end:
    _KERNEL_SET_PSW(psw);
    _KERNEL_ASSERT_TER_TSK();
    return r;
}

/* ── Task sleep/wakeup ─────────────────────────────────── */

ER slp_tsk(void)
{
    UINT   psw;
    T_TCB *tcb = (T_TCB*)_kernel_cur;

    psw = _KERNEL_DIS_PSW();
    if (tcb->wup) {
        tcb->wup--;
    } else {
        _kernel_wai(tcb, TMO_FEVR, &_kernel_dmy_que, 0);
    }
    _KERNEL_SET_PSW(psw);
    _KERNEL_ASSERT_SLP_TSK();
    return E_OK;
}

ER wup_tsk(ID tskid)
{
    UINT   psw;
    ER     r = E_OK;
    T_TCB *tcb;

    if (tskid < 1 || tskid > DSO_MAX_TSK) return E_ID;
    tcb = _kernel_tcb[tskid - 1];
    if (!tcb) return E_NOEXS;

    psw = _KERNEL_DIS_PSW();

    if (tcb->sts & TTS_WAI) {
        _kernel_deq_rdy_dsp(tcb);
    } else if (tcb->sts == TTS_RDY || tcb->sts == TTS_RUN) {
        tcb->wup++;
    } else if (tcb->sts == TTS_DMT) {
        r = E_OBJ;
        goto end;
    } else {
        r = E_SYS;
        goto end;
    }

end:
    _KERNEL_SET_PSW(psw);
    _KERNEL_ASSERT_WUP_TSK();
    return r;
}

ER can_wup(ID tskid)
{
    UINT   psw;
    ER     r = 0;
    T_TCB *tcb;

    if (tskid < 1 || tskid > DSO_MAX_TSK) return E_ID;
    tcb = _kernel_tcb[tskid - 1];
    if (!tcb) return E_NOEXS;

    psw = _KERNEL_DIS_PSW();
    r = tcb->wup;
    tcb->wup = 0;
    _KERNEL_SET_PSW(psw);
    _KERNEL_ASSERT_CAN_WUP();
    return r;
}
