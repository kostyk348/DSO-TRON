/* DSO-TRON x86_64 Boot Loader */

.set PAGE_PRESENT,     0x01
.set PAGE_RW,          0x02
.set PAGE_HUGE,        0x80
.set PAGE_SIZE,        0x200000

.section .text.boot, "ax"
.align 8
.global _start
_start:
    jmp     _entry32

    /* ── Multiboot2 Header ─────────────────────────────── */
.align 8
_mb_start:
    .long 0xE85250D6               /* magic */
    .long 0                         /* i386 */
    .long _mb_end - _mb_start       /* size */
    .long -(0xE85250D6 + 0 + (_mb_end - _mb_start)) /* checksum */
    .word 0                         /* end tag type */
    .word 0                         /* end tag flags */
    .int 8                          /* end tag size */
_mb_end:

    /* ── GDT (aligned, close to entry) ─────────────────── */
.align 16
_gdt64:
    .quad 0x0000000000000000
    .quad 0x00AF9A000000FFFF
    .quad 0x00AF92000000FFFF
_gdt64_end:

_gdt_ptr:
    .word _gdt64_end - _gdt64 - 1
    .long _gdt64

    /* ── 32-bit entry ──────────────────────────────────── */
    .code32
_entry32:
    movl    %eax, %edi
    movl    %ebx, %esi
    cli

    movl    %cr0, %eax
    andl    $0x9FFFFFFF, %eax
    movl    %eax, %cr0

    leal    _end - 0x4000, %esp
    movl    %esp, %ebp

    leal    _pml4, %eax
    leal    _pdp,  %ecx
    orl     $(PAGE_PRESENT | PAGE_RW), %ecx
    movl    %ecx, (%eax)

    leal    _pd,   %ecx
    orl     $(PAGE_PRESENT | PAGE_RW), %ecx
    leal    _pdp,  %ebx
    movl    %ecx, (%ebx)

    leal    _pd,   %ecx
    orl     $(PAGE_PRESENT | PAGE_RW), %ecx
    leal    _pdp,  %ebx
    movl    %ecx, 8(%ebx)     /* PDP[1] for KERNEL_BASE */

    leal    _pd, %ecx
    xorl    %eax, %eax
    movl    $512, %edx
1:
    movl    %eax, %ebx
    orl     $(PAGE_PRESENT | PAGE_RW | PAGE_HUGE), %ebx
    movl    %ebx, (%ecx)
    addl    $PAGE_SIZE, %eax
    addl    $8, %ecx
    subl    $1, %edx
    jnz     1b

    movl    %cr4, %eax
    orl     $(1 << 5), %eax
    movl    %eax, %cr4

    leal    _pml4, %eax
    movl    %eax, %cr3

    movl    $0xC0000080, %ecx
    rdmsr
    orl     $(1 << 8), %eax
    wrmsr

    /* Load GDT and enable paging */
    lgdt    _gdt_ptr

    movl    %cr0, %eax
    orl     $(1 << 31), %eax
    movl    %eax, %cr0

    /* Far jump to 64-bit mode with our GDT CS */
    .byte 0xEA
    .long .L64
    .word 0x08

.code64
.L64:
    movw    $0x10, %ax
    movw    %ax, %ds
    movw    %ax, %es
    movw    %ax, %fs
    movw    %ax, %gs
    movw    %ax, %ss

    /* Stack at _end (physical 0x115000+), well above BIOS ROM at 0xF0000 */
    leaq    _end(%rip), %rsp
    movq    %rsp, %rbp

    movl    %edi, %eax
    movl    %esi, %edx

    movabsq $kernel_main, %rbx
    callq   *%rbx

    cli
    hlt

/* ── Page Tables ───────────────────────────────────────── */
.align 4096
_pml4:
    .space 4096
_pdp:
    .space 4096
_pd:
    .space 4096
