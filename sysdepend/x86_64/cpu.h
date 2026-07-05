/*
 * cpu.h — x86_64 Architecture-Specific Macros for DSO-TRON
 * ==========================================================
 *
 * Заменяет URIBO h8300h/kernel_s.h.
 *
 * Переопределяет μITRON-зависимые макросы для x86_64:
 *   _KERNEL_DIS_PSW        — cli; return old RFLAGS
 *   _KERNEL_SET_PSW        — write RFLAGS
 *   _KERNEL_INI_CTX        — инициализация контекста задачи на стеке
 *   _KERNEL_SW             — контекстное переключение
 *   _KERNEL_PANIC          — аварийная остановка
 *   _KERNEL_SET_TCB_RET    — установка возвращаемого значения системного вызова
 *   _KERNEL_GET_TCB_RET    — чтение возвращаемого значения
 *   _KERNEL_SET_TCB_DAT    — установка данных в TCB
 *   _KERNEL_GET_TCB_DAT    — чтение данных из TCB
 *   ... остальные TCB геттеры/сеттеры
 */

#ifndef _CPU_H_
#define _CPU_H_

#include <stdint.h>

/* ── CPU State save/restore (cli/sti via RFLAGS) ────────── */

static inline uint32_t _KERNEL_DIS_PSW(void)
{
    uint64_t rflags;
    __asm__ volatile(
        "pushfq; popq %0; cli"
        : "=r"(rflags)
        :
        : "memory"
    );
    return (uint32_t)rflags;
}

static inline void _KERNEL_SET_PSW(uint32_t psw)
{
    (void)psw;
    /* DSO: просто разрешаем прерывания.
     * В TT-режиме interrupts всегда разрешены вне критических секций. */
    __asm__ volatile("sti" ::: "memory");
}

/* ── Task context initialization ────────────────────────── */

#define _KERNEL_INI_CTX() \
do { \
    uint64_t *_sp_; \
    _sp_ = (uint64_t*)((char*)(tcb)->stk + (tcb)->ctsk->stksz); \
    *--_sp_ = (uint64_t)ext_tsk;   /* return to ext_tsk when task exits */ \
    *--_sp_ = 0;                    /* rbp = 0 */ \
    *--_sp_ = 0;                    /* rbx = 0 */ \
    *--_sp_ = 0;                    /* r12 = 0 */ \
    *--_sp_ = 0;                    /* r13 = 0 */ \
    *--_sp_ = 0;                    /* r14 = 0 */ \
    *--_sp_ = (uint64_t)(tcb)->ctsk->task;  /* r15 = entry point */ \
    (tcb)->sp = _sp_; \
} while(0)

/* ── Context switch ─────────────────────────────────────── */

extern void _kernel_sw(T_TCB *new);

#define _KERNEL_SW(new) _kernel_sw(new)

/* ── Kernel panic ───────────────────────────────────────── */

#define _KERNEL_PANIC() \
do { \
    __asm__ volatile("cli; hlt" ::: "memory"); \
    while(1) {} \
} while(0)

/* ── TCB register value accessors ───────────────────────── */

/* These replace URIBO's stack-frame register access.
 * In DSO, values are stored in TCB fields (ret, dat, siz, mod, ptn, rdv). */

#define _KERNEL_SET_TCB_RET(tcb_, ret_)  ((tcb_)->ret = (ER)(ret_))
#define _KERNEL_GET_TCB_RET(tcb_)        ((ER)(tcb_)->ret)

#define _KERNEL_SET_TCB_DAT(tcb_, dat_)  ((tcb_)->dat = (VP_INT)(dat_))
#define _KERNEL_GET_TCB_DAT(tcb_)        ((VP_INT)(tcb_)->dat)

#define _KERNEL_SET_TCB_SIZ(tcb_, siz_)  ((tcb_)->siz = (SIZE)(siz_))
#define _KERNEL_GET_TCB_SIZ(tcb_)        ((SIZE)(tcb_)->siz)

#define _KERNEL_SET_TCB_MOD(tcb_, mod_)  ((tcb_)->mod = (MODE)(mod_))
#define _KERNEL_GET_TCB_MOD(tcb_)        ((MODE)(tcb_)->mod)

#define _KERNEL_SET_TCB_PTN(tcb_, ptn_)  ((tcb_)->ptn = (FLGPTN)(ptn_))
#define _KERNEL_GET_TCB_PTN(tcb_)        ((FLGPTN)(tcb_)->ptn)

#define _KERNEL_SET_TCB_RDV(tcb_, rdv_)  ((tcb_)->rdv = (RDVNO)(rdv_))
#define _KERNEL_GET_TCB_RDV(tcb_)        ((RDVNO)(tcb_)->rdv)

/* ── Misc architecture macros ───────────────────────────── */

#define TMSK_ALIGN 1

/* Memory barrier — full fence */
#define _KERNEL_MF() __asm__ volatile("mfence" ::: "memory")

/* NOP gap for interrupt */
#define _KERNEL_INTGAP() __asm__ volatile("nop")

#endif /* _CPU_H_ */
