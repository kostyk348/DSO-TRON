/*
 * PIT Timer — DSO-TRON Time Source
 * =================================
 * Единственный источник времени и прерываний.
 * Настраивается на DSO_TICK_HZ (100 Hz по умолчанию).
 * Вектор IRQ0 → PIT-обработчик, который тикает TT-планировщик.
 */

#include <stdint.h>
#include "../../include/dso_config.h"

/* PIT I/O ports */
#define PIT_CH0     0x40
#define PIT_CMD     0x43

#define PIT_DIVISOR (1193182 / DSO_TICK_HZ)

extern void dso_tick_handler(void);

void pit_init(void)
{
    uint16_t div = (uint16_t)PIT_DIVISOR;

    __asm__ volatile("outb %0, %1" : : "a"((uint8_t)0x36), "N"(PIT_CMD));
    __asm__ volatile("outb %0, %1" : : "a"((uint8_t)(div & 0xFF)), "N"(PIT_CH0));
    __asm__ volatile("outb %0, %1" : : "a"((uint8_t)(div >> 8)), "N"(PIT_CH0));
}

/* PIT interrupt handler — uses GCC interrupt attribute for proper iretq */
struct interrupt_frame;
__attribute__((interrupt))
void pit_handler(struct interrupt_frame *frame)
{
    (void)frame;
    dso_tick_handler();
    __asm__ volatile("movb $0x20, %%al; outb %%al, $0x20" ::: "al", "memory");
}
