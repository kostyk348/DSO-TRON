#include <stdint.h>

#define COM1 0x3F8

static void outb(uint16_t port, uint8_t val)
{
    __asm__ volatile("outb %0, %1" : : "a"(val), "Nd"(port));
}

static uint8_t inb(uint16_t port)
{
    uint8_t ret;
    __asm__ volatile("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

void serial_init(void)
{
    outb(COM1 + 1, 0x00);
    outb(COM1 + 3, 0x80);
    outb(COM1 + 0, 0x01);
    outb(COM1 + 1, 0x00);
    outb(COM1 + 3, 0x03);
    outb(COM1 + 2, 0xC7);
    outb(COM1 + 4, 0x0B);
}

void serial_putc(char c)
{
    for (int i = 0; i < 10000; i++) {
        if (inb(COM1 + 5) & 0x20) break;
    }
    outb(COM1, c);
}

void serial_puts(const char *s)
{
    while (*s) serial_putc(*s++);
}

static const char _hex[] = "0123456789ABCDEF";

void serial_hex32(uint32_t val)
{
    serial_putc('0');
    serial_putc('x');
    for (int i = 28; i >= 0; i -= 4)
        serial_putc(_hex[(val >> i) & 0xF]);
}
