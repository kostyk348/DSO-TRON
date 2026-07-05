/*
 * GDT — DSO-TRON
 * ===============
 * Минимальная GDT: null, 64-bit code, data.
 * Должна совпадать с GDT из boot.S (L=1, G=1, limit=0xFFFFF).
 */

#include <stdint.h>

typedef struct {
    uint16_t limit_low;
    uint16_t base_low;
    uint8_t  base_mid;
    uint8_t  access;
    uint8_t  flags_limit;
    uint8_t  base_high;
} __attribute__((packed)) gdt_entry;

typedef struct {
    uint16_t limit;
    uint64_t base;
} __attribute__((packed)) gdtr;

static gdt_entry gdt[3];
static gdtr     _gdtr;

#define GDT_CODE  0x00AF9A000000FFFFULL
#define GDT_DATA  0x00AF92000000FFFFULL

void gdt_init(void)
{
    /* Null */
    gdt[0] = (gdt_entry){0, 0, 0, 0, 0, 0};

    /* 64-bit code: match boot.S GDT */
    gdt[1].limit_low = 0xFFFF;
    gdt[1].base_low  = 0;
    gdt[1].base_mid  = 0;
    gdt[1].access    = 0x9A;
    gdt[1].flags_limit = 0xAF;
    gdt[1].base_high = 0;

    /* Data: match boot.S GDT */
    gdt[2].limit_low = 0xFFFF;
    gdt[2].base_low  = 0;
    gdt[2].base_mid  = 0;
    gdt[2].access    = 0x92;
    gdt[2].flags_limit = 0xAF;
    gdt[2].base_high = 0;

    _gdtr.limit = sizeof(gdt) - 1;
    _gdtr.base  = (uint64_t)&gdt;

    __asm__ volatile("lgdt %0" :: "m"(_gdtr) : "memory");

    /* Reload segments */
    __asm__ volatile(
        "pushq $0x08\n\t"
        "leaq  1f(%%rip), %%rax\n\t"
        "pushq %%rax\n\t"
        "lretq\n"
        "1:\n\t"
        "movw  $0x10, %%ax\n\t"
        "movw  %%ax, %%ds\n\t"
        "movw  %%ax, %%es\n\t"
        "movw  %%ax, %%ss\n\t"
        :::
        "rax", "memory"
    );
}
