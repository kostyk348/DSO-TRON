#ifndef _KERNEL_H_
#define _KERNEL_H_
/*
 * DSO-TRON Kernel Header
 * =======================
 * Минимальный kernel.h — заменяет URIBO kernel.h + itron.h + config.h.
 * Определяет только то, что нужно для DSO-режима.
 *
 * Все объекты статические. Никакого mpl/mpf/malloc.
 * TT-планировщик вместо приоритетной очереди.
 * Lock-free SPSC FIFO вместо dtq с мьютексами.
 */

#include <stddef.h>
#include <stdint.h>

#ifndef BOOL
#define BOOL int
#endif
#ifndef TRUE
#define TRUE 1
#endif
#ifndef FALSE
#define FALSE 0
#endif

/* ── μITRON-совместимые типы ──────────────────────────── */

typedef int32_t  INT;
typedef uint32_t UINT;
typedef uint8_t  UB;
typedef uint16_t UH;
typedef int8_t   B;
typedef uint32_t STAT;
typedef int32_t  ER;
typedef uint32_t ATR;
typedef uint32_t ID;
typedef uint16_t PRI;
typedef int32_t  TMO;
typedef uint32_t SIZE;
typedef uintptr_t VP;
typedef int32_t   VP_INT;
typedef void     FP;
typedef uint32_t FLGPTN;
typedef uint32_t TEXPTN;
typedef uint32_t RDVPTN;
typedef uint32_t MODE;
typedef uint32_t RDVNO;

/* ── System time structure ─────────────────────────────── */
typedef struct {
    uint32_t ltime;  /* low 32 bits */
    int32_t  utime;  /* high 32 bits */
} SYSTIM;

/* ── Constants ─────────────────────────────────────────── */

#define E_OK    0
#define TSK_SELF 0
#define TMO_FEVR ((TMO)-1)
#define TMO_POL  0

/* Task exception */
#define TTEX_DIS 0x0000
#define TTEX_ENA 0x0001

/* Task states */
#define TTS_RUN 0x01
#define TTS_RDY 0x02
#define TTS_WAI 0x04
#define TTS_SUS 0x08
#define TTS_WAS 0x0C
#define TTS_DMT 0x10

/* Wait types */
#define TTW_SLP  0x0001
#define TTW_DLY  0x0002
#define TTW_SDTQ 0x0010
#define TTW_RDTQ 0x0020
#define TTW_MPF  0x2000
#define TTW_MPL  0x4000

/* Attribute bits */
#define TA_NULL    0x00
#define TA_HLNG    0x00
#define TA_ASM     0x01
#define TA_TFIFO   0x00
#define TA_TPRI    0x01
#define TA_ACT     0x02

/* Error codes */
#define E_SYS   ((ER)-5)
#define E_NOSPT ((ER)-9)
#define E_RSFN  ((ER)-10)
#define E_PAR   ((ER)-17)
#define E_ID    ((ER)-18)
#define E_CTX   ((ER)-25)
#define E_OACV  ((ER)-27)
#define E_ILUSE ((ER)-28)
#define E_NOMEM ((ER)-33)
#define E_NOID  ((ER)-34)
#define E_OBJ   ((ER)-41)
#define E_NOEXS ((ER)-42)
#define E_QOVR  ((ER)-43)
#define E_RLWAI ((ER)-49)
#define E_TMOUT ((ER)-50)
#define E_DLT   ((ER)-51)

/* ── Kernel status ─────────────────────────────────────── */
#define T_KER_SYS   0x01  /* system mode */
#define T_KER_DIS   0x04  /* dispatch disabled */
#define T_KER_ISR   0x08  /* in interrupt handler */
#define T_KER_ISLD  0x0C  /* in ISR or dispatch disabled */

/* ── Linked list (from URIBO) ──────────────────────────── */

typedef struct t_lnk {
    struct t_lnk *n;
    struct t_lnk *p;
} T_LNK;

/* ── Timer control block (minimal) ─────────────────────── */

typedef struct t_tim {
    struct t_tim *n;
    struct t_tim *p;
    TMO           tmo;
} T_TIM;

/* ── Task Control Block ────────────────────────────────── */

typedef struct t_tex {
    TEXPTN ptn;
    STAT   stat;
} T_TEX;

typedef struct t_ctsk {
    ATR    tskatr;
    VP_INT exinf;
    FP    *task;
    PRI    itskpri;
    SIZE   stksz;
    VP     stk;
} T_CTSK;

typedef struct t_rtsk {
    STAT   tskstat;
    PRI    tskpri;
    PRI    tskbpri;
    STAT   tskwait;
    ID     wobjid;
    TMO    lefttmo;
    UINT   actcnt;
    UINT   wupcnt;
    UINT   suscnt;
} T_RTSK;

typedef struct t_cdtq {
    ATR    dtqatr;
    UINT   dtqcnt;
    UINT   dsksize;
    char  *name;
} T_CDTQ;

typedef struct t_rdtq {
    UINT   dtqcnt;
    ID     wtskid;
    ID     smtxid;
} T_RDTQ;

typedef struct t_tcb {
    struct t_tcb *n;
    struct t_tcb *p;
    void         *sp;          /* saved stack pointer */
    T_TEX        *tex;
    T_TIM         tim;
    UB            id;
    PRI           pri;
    UB            sts;
    UB            act;
    UB            wup;
    UB            sus;
    STAT          wai;
    ID            wid;
    VP            stk;
    T_CTSK       *ctsk;
    /* Register context values (μITRON compat: set/get from TCB instead of stack) */
    ER            ret;         /* return value (er0) */
    VP_INT        dat;         /* data (er1) */
    SIZE          siz;         /* size (er2) */
    MODE          mod;         /* mode (er2) */
    FLGPTN        ptn;         /* pattern (er3.h) */
    RDVNO         rdv;         /* rendezvous number (er3.l) */
} T_TCB;

/* ── Dispatch macros (must be defined per arch) ────────── */

extern volatile T_TCB *_kernel_cur;
extern volatile UB     _kernel_sts;
extern volatile UB     _kernel_dly;
extern volatile TMO    _kernel_sysclk;
extern volatile SYSTIM _kernel_systim;
extern T_LNK           _kernel_dmy_que;
extern T_LNK           _kernel_rdq[];
extern T_TCB          *_kernel_tcb[];

#define _KERNEL_CHK_IL(s)   ((s) & T_KER_ISR)
#define _KERNEL_CHK_IS(s)   ((s) & T_KER_SYS)
#define _KERNEL_CHK_TSK(s)  (!((s) & (T_KER_SYS|T_KER_ISR)))
#define _KERNEL_CHK_ISLD(s) ((s) & T_KER_ISLD)

/* These must be implemented in arch-specific code (cpu.h) */
#include "../sysdepend/x86_64/cpu.h"

/* ── DSO globals ───────────────────────────────────────── */

void _kernel_nul_que(T_LNK *que, PRI pri);
void _kernel_enq(T_LNK *que, T_LNK *add);
void _kernel_deq(T_LNK *del);
T_LNK *_kernel_first(T_LNK *que, PRI pri);
void _kernel_run(T_TCB *new);
void _kernel_higher(T_TCB *new);
void _kernel_highest(void);
ER   _kernel_wai(T_TCB *tcb, TMO tmout, T_LNK *que, BOOL pri);
void _kernel_deq_rdy_dsp(T_TCB *tcb);
B    _kernel_deq_rdy(T_TCB *tcb);
void _kernel_dlt(T_LNK *que, PRI pri);
void _kernel_act(T_TCB *tcb, VP_INT arg);
T_TCB *_kernel_tcb_alloc(void);

/* ── μITRON system calls ───────────────────────────────── */

/* Task management */
ER cre_tsk(ID tskid, const T_CTSK *pk_ctsk);
ER sta_tsk(ID tskid, VP_INT stacd);
ER ext_tsk(void);
ER exd_tsk(void);
ER ter_tsk(ID tskid);
ER slp_tsk(void);
ER wup_tsk(ID tskid);
ER can_wup(ID tskid);
ER get_tid(ID *p_tskid);
ER get_pri(ID tskid, PRI *p_tskpri);
ER chg_pri(ID tskid, PRI tskpri);

/* Task synchronization */
ER wup_tsk(ID tskid);
ER can_wup(ID tskid);
ER slp_tsk(void);
ER dly_tsk(TMO dlytim);

/* Data queue (DSO: lock-free SPSC FIFO) */
ER cre_dtq(ID dtqid, const T_CDTQ *pk_cdtq);
ER snd_dtq(ID dtqid, VP_INT data);
ER fsnd_dtq(ID dtqid, VP_INT data);
ER rcv_dtq(ID dtqid, VP_INT *p_data);
ER trcv_dtq(ID dtqid, VP_INT *p_data, TMO tmout);
ER ref_dtq(ID dtqid, T_RDTQ *pk_rdtq);

/* Time management */
ER get_tim(SYSTIM *pk_tim);
ER set_tim(const SYSTIM *pk_tim);

/* System */
ER ref_ver(void);
ER ref_sys(void);

/* ── DSO-TRON extensions ───────────────────────────────── */

/* (declared in include/dso.h) */

/* Debug/assert */
#ifdef _KERNEL_ASSERT
#define _KERNEL_ASSERT_CRE_TSK()
#define _KERNEL_ASSERT_STA_TSK()
#define _KERNEL_ASSERT_EXD_TSK()
#define _KERNEL_ASSERT_TER_TSK()
#define _KERNEL_ASSERT_SLP_TSK()
#define _KERNEL_ASSERT_WUP_TSK()
#define _KERNEL_ASSERT_CAN_WUP()
#define _KERNEL_ASSERT_SND_DTQ()
#define _KERNEL_ASSERT_RCV_DTQ()
#define _KERNEL_ASSERT_REF_DTQ()
#define _KERNEL_ASSERT_CRE_DTQ()
#else
#define _KERNEL_ASSERT_CRE_TSK()
#define _KERNEL_ASSERT_STA_TSK()
#define _KERNEL_ASSERT_EXD_TSK()
#define _KERNEL_ASSERT_TER_TSK()
#define _KERNEL_ASSERT_SLP_TSK()
#define _KERNEL_ASSERT_WUP_TSK()
#define _KERNEL_ASSERT_CAN_WUP()
#define _KERNEL_ASSERT_SND_DTQ()
#define _KERNEL_ASSERT_RCV_DTQ()
#define _KERNEL_ASSERT_REF_DTQ()
#define _KERNEL_ASSERT_CRE_DTQ()
#endif

#define _KERNEL_RET(er) do { return (er); } while(0)
#define _KERNEL_END(er) do { r = (er); goto end; } while(0)

#endif
