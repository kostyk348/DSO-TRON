#ifndef DSO_H
#define DSO_H

/*
 * DSO-TRON — Deterministic Systems Optimization for μITRON
 * ==========================================================
 *
 * DSO-принципы:
 *   1. ВСЁ статическое — ни одного malloc в рантайме
 *   2. Lock-free IPC — SPSC FIFO вместо dtq с мьютексами
 *   3. Time-Triggered — никаких приоритетов, только расписание
 *   4. Resource Contracts — каждый слот знает свой бюджет циклов
 *
 * Это единственный заголовок, который нужно включить в μITRON-задачу.
 */

#include "dso_arena.h"
#include "dso_fifo.h"
#include "dso_sched.h"

/*
 * DSO Resource Contract для μITRON-задачи.
 * Каждая задача при создании (cre_tsk) получает слот в TT-расписании
 * и свой Resource Contract.
 */
#define DSO_CONTRACT_DEFAULT { 500, 256, 100 }

/*
 * DSO-версия cre_tsk — создаёт задачу в Static Arena,
 * регистрирует её в Time-Triggered расписании.
 * Вместо динамического TCB из malloc — предварительно выделенный.
 */
ER dso_cre_tsk(ID tskid, const T_CTSK *pk_ctsk);

/*
 * DSO-версия IPC — lock-free data queue.
 * Никаких мьютексов, никакой динамической памяти.
 * Использует dso_fifo внутри.
 */
ER dso_cre_dtq(ID dtqid, const T_CDTQ *pk_cdtq);
ER dso_snd_dtq(ID dtqid, VP_INT data);
ER dso_rcv_dtq(ID dtqid, VP_INT *p_data);

/*
 * DSO-таймер — единственный Time Source.
 * PIT генерирует тики, TT-планировщик диспатчит задачи по расписанию.
 * Никаких асинхронных IRQ, только TIMER.
 */
void dso_tick_handler(void);

/* DSO kernel initialization and scheduler registration */
void dso_kernel_init(void);
void dso_sched_register(const dso_slot *slots, uint32_t num);

#endif
