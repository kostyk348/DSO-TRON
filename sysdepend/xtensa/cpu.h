#ifndef _CPU_H_
#define _CPU_H_

#include <stdint.h>
#include <xtensa/config/core.h>

static inline uint32_t _KERNEL_DIS_PSW(void)
{
    uint32_t state;
    __asm__ volatile("rsil %0, %1" : "=r"(state) : "i"(15));
    return state;
}

static inline void _KERNEL_SET_PSW(uint32_t psw)
{
    __asm__ volatile("wsr %0, PS; rsync" : : "r"(psw));
}

#define _KERNEL_INI_CTX() ((void)0)

extern void _kernel_sw(T_TCB *new);
#define _KERNEL_SW(new) _kernel_sw(new)

#define _KERNEL_PANIC() \
do { \
    __asm__ volatile("wsr %0, PS; rsync" : : "r"(0)); \
    while(1) {} \
} while(0)

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

#define TMSK_ALIGN 1
#define _KERNEL_MF() __asm__ volatile("memw" ::: "memory")
#define _KERNEL_INTGAP() __asm__ volatile("nop")

#endif
