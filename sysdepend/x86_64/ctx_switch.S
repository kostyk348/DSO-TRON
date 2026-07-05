/* DSO-TRON x86_64 Context Switch
 *
 * _kernel_ctx_switch(old_sp, new_sp)
 *   — Save current context, load new context, return.
 *   Used for initial task activation only.
 *
 * _kernel_sw(tcb)
 *   — μITRON-compatible context switch.
 *     Save current task context to _kernel_cur->sp,
 *     set _kernel_cur = tcb,
 *     restore tcb->sp.
 *
 * _kernel_ctx_init(stack_top, entry, sp_out)
 *   — Set up initial stack frame for a new task.
 */

.text
.code64

/* ── _kernel_ctx_switch(uint64_t *old_sp, uint64_t new_sp) ── */
.global _kernel_ctx_switch
_kernel_ctx_switch:
    pushq   %rbp
    pushq   %rbx
    pushq   %r12
    pushq   %r13
    pushq   %r14
    pushq   %r15
    movq    %rsp, (%rdi)       /* *old_sp = rsp */
    movq    %rsi, %rsp         /* rsp = new_sp */
    popq    %r15
    popq    %r14
    popq    %r13
    popq    %r12
    popq    %rbx
    popq    %rbp
    ret

/* ── _kernel_ctx_init(stack_top, entry, sp_out) ───────────── */
/*   rdi = stack_top, rsi = entry, rdx = &sp_out (uint64_t*) */
.global _kernel_ctx_init
_kernel_ctx_init:
    movq    %rdi, %rax         /* rax = stack_top */
    subq    $56, %rax          /* make room for 7 regs (56 bytes) */

    /* Set up initial context: [ret_addr, r15, r14,..., rbp] */
    movq    %rsi, 48(%rax)     /* ret_addr = entry */
    movq    $0,   40(%rax)     /* rbp = 0 */
    movq    $0,   32(%rax)     /* rbx = 0 */
    movq    $0,   24(%rax)     /* r12 = 0 */
    movq    $0,   16(%rax)     /* r13 = 0 */
    movq    $0,   8(%rax)      /* r14 = 0 */
    movq    $0,   0(%rax)      /* r15 = 0 */

    movq    %rax, (%rdx)       /* *sp_out = new stack pointer */
    ret

/* ── _kernel_sw(T_TCB *new) ───────────────────────────────── */
/*   rdi = new TCB to switch to */
.global _kernel_sw
_kernel_sw:
    /* Clear _kernel_dly */
    movb    $0, _kernel_dly(%rip)

    /* If new == _kernel_cur, skip */
    movq    _kernel_cur(%rip), %rax
    cmpq    %rax, %rdi
    je      .Lsw_return

    /* Save current context if _kernel_cur != NULL */
    testq   %rax, %rax
    jz      .Lsw_dispatch

    /* Push callee-saved regs + return address */
    pushq   %rbp
    pushq   %rbx
    pushq   %r12
    pushq   %r13
    pushq   %r14
    pushq   %r15
    movq    %rsp, TCB_SP_OFF(%rax)  /* _kernel_cur->sp = rsp */

.Lsw_dispatch:
    /* If new == NULL, go idle */
    testq   %rdi, %rdi
    jz      .Lsw_idle

    /* _kernel_cur = new */
    movq    %rdi, _kernel_cur(%rip)

    /* rsp = new->sp */
    movq    TCB_SP_OFF(%rdi), %rsp

    /* Restore callee-saved regs */
    popq    %r15
    popq    %r14
    popq    %r13
    popq    %r12
    popq    %rbx
    popq    %rbp
    ret

.Lsw_return:
    ret

.Lsw_idle:
    /* In DSO, idle = hlt loop with interrupts enabled */
    movq    $0, _kernel_cur(%rip)
    sti
.Lsw_idle_loop:
    hlt
    jmp     .Lsw_idle_loop

/* TCB member offsets */
.set TCB_SP_OFF, 8
