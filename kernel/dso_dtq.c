/*
 * dso_dtq.c — DSO-TRON Data Queue (Lock-free SPSC FIFO)
 * =======================================================
 *
 * Замена cre_dtq, snd_dtq, rcv_dtq, fsnd_dtq, trcv_dtq из URIBO.
 *
 * Отличия от μITRON:
 *   - Lock-free SPSC: никаких мьютексов, прерываний, спин-блокировок
 *   - Фиксированный размер: dtqcnt задаётся DSO_DTQ_SIZE compile-time
 *   - Неблокирующий API: все операции возвращают E_QOVR/E_TMOUT
 *   - Никаких очередей ожидания (waiting queues) — TT не блокируется
 *
 * Заменяет struct t_dtq из URIBO:
 *   typedef struct t_dtq {
 *       UINT   ip;          // in index
 *       UINT   op;          // out index
 *       UINT   cnt;         // data count
 *       VP_INT *buf;        // ring buffer (malloc!)
 *       T_CDTQ *cdtq;
 *       T_LNK  rque;        // receive queue
 *       T_LNK  sque[1];     // send queue
 *   } T_DTQ;
 *
 * DSO-версия:
 *   T_DTQ содержит только dso_fifo + cdtq.
 *   Нет rque/sque — неблокирующие операции не ждут.
 *   Нет malloc для buf — fifo статический.
 */

#include "kernel.h"
#include "../include/dso.h"
#include "../include/dso_config.h"

/* ── Static DTQ Storage ────────────────────────────────── */

#define DSO_DTQ_ELEMENT_SIZE sizeof(VP_INT)

static struct {
    dso_fifo fifo;
    uint8_t  buf[DSO_DTQ_SIZE * DSO_DTQ_ELEMENT_SIZE];
    T_CDTQ  *cdtq;
} _dso_dtq_table[DSO_MAX_DTQ];

/* ── cre_dtq — Static (no malloc) ──────────────────────── */

ER cre_dtq(ID dtqid, const T_CDTQ *pk_cdtq)
{
    UINT psw;
    ER   r = E_OK;

#ifndef _KERNEL_NO_STATIC_ERR
    {
        UB s = _kernel_sts;
        if (_KERNEL_CHK_IL(s)) _KERNEL_RET(E_CTX);
        if (dtqid < 1 || dtqid > DSO_MAX_DTQ) _KERNEL_RET(E_ID);
        if (!pk_cdtq) _KERNEL_RET(E_PAR);
    }
#endif

    psw = _KERNEL_DIS_PSW();

    uint32_t idx = dtqid - 1;
    dso_fifo_init(&_dso_dtq_table[idx].fifo,
                   _dso_dtq_table[idx].buf,
                   DSO_DTQ_ELEMENT_SIZE,
                   DSO_DTQ_SIZE);
    _dso_dtq_table[idx].cdtq = (T_CDTQ*)pk_cdtq;

    _KERNEL_SET_PSW(psw);
    _KERNEL_ASSERT_CRE_DTQ();
    return r;
}

/* ── snd_dtq — Lock-free Send (Producer) ───────────────── */

ER snd_dtq(ID dtqid, VP_INT data)
{
    if (dtqid < 1 || dtqid > DSO_MAX_DTQ) return E_ID;

    dso_fifo *q = &_dso_dtq_table[dtqid - 1].fifo;
    if (dso_fifo_push(q, &data) != 0)
        return E_QOVR;

    _KERNEL_ASSERT_SND_DTQ();
    return E_OK;
}

/* ── fsnd_dtq — Fast Send (same as snd in DSO) ────────── */

ER fsnd_dtq(ID dtqid, VP_INT data)
{
    return snd_dtq(dtqid, data);
}

/* ── rcv_dtq — Lock-free Receive (Consumer) ────────────── */

ER rcv_dtq(ID dtqid, VP_INT *p_data)
{
    if (dtqid < 1 || dtqid > DSO_MAX_DTQ) return E_ID;
    if (!p_data) return E_PAR;

    dso_fifo *q = &_dso_dtq_table[dtqid - 1].fifo;
    if (dso_fifo_pop(q, p_data) != 0)
        return E_TMOUT;

    _KERNEL_ASSERT_RCV_DTQ();
    return E_OK;
}

/* ── trcv_dtq — Timeout Receive (kept for compat) ──────── */

ER trcv_dtq(ID dtqid, VP_INT *p_data, TMO tmout)
{
    (void)tmout;
    return rcv_dtq(dtqid, p_data);
}

/* ── ref_dtq — Reference data queue status ─────────────── */

ER ref_dtq(ID dtqid, T_RDTQ *pk_rdtq)
{
    if (dtqid < 1 || dtqid > DSO_MAX_DTQ) return E_ID;
    if (!pk_rdtq) return E_PAR;

    dso_fifo *q = &_dso_dtq_table[dtqid - 1].fifo;
    pk_rdtq->dtqcnt = dso_fifo_len(q);
    pk_rdtq->wtskid = 0;  /* DSO: нет waiting tasks */
    pk_rdtq->smtxid = 0;

    _KERNEL_ASSERT_REF_DTQ();
    return E_OK;
}
