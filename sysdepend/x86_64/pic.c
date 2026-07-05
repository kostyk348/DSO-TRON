/*
 * PIC (8259A) — DSO-TRON
 * =======================
 */

#include <stdint.h>

#define PIC1_CMD  0x20
#define PIC1_DATA 0x21
#define PIC2_CMD  0xA0
#define PIC2_DATA 0xA1

void pic_init(void)
{
    /* ICW1: init */
    __asm__ volatile("outb %%al, %0" :: "N"(PIC1_CMD), "a"((uint8_t)0x11));
    __asm__ volatile("outb %%al, %0" :: "N"(PIC2_CMD), "a"((uint8_t)0x11));

    /* ICW2: vector offsets: IRQ0 → INT 0x20, IRQ8 → INT 0x28 */
    __asm__ volatile("outb %%al, %0" :: "N"(PIC1_DATA), "a"((uint8_t)0x20));
    __asm__ volatile("outb %%al, %0" :: "N"(PIC2_DATA), "a"((uint8_t)0x28));

    /* ICW3: master/slave connections */
    __asm__ volatile("outb %%al, %0" :: "N"(PIC1_DATA), "a"((uint8_t)4));
    __asm__ volatile("outb %%al, %0" :: "N"(PIC2_DATA), "a"((uint8_t)2));

    /* ICW4: 8086 mode */
    __asm__ volatile("outb %%al, %0" :: "N"(PIC1_DATA), "a"((uint8_t)1));
    __asm__ volatile("outb %%al, %0" :: "N"(PIC2_DATA), "a"((uint8_t)1));

    /* Mask all interrupts except timer */
    __asm__ volatile("outb %%al, %0" :: "N"(PIC1_DATA), "a"((uint8_t)~1));
    __asm__ volatile("outb %%al, %0" :: "N"(PIC2_DATA), "a"((uint8_t)0xFF));
}

/* Remap PIT timer only */
void pit_unmask(void)
{
    __asm__ volatile("outb %%al, %0" :: "N"(PIC1_DATA), "a"((uint8_t)~1));
}
