#include "esp_rom_sys.h"
#include <stdint.h>

void serial_init(void) {}

void serial_putc(char c)
{
    esp_rom_uart_putc(c);
}

void serial_puts(const char *s)
{
    for (; *s; s++)
        serial_putc(*s);
}

static const char _hex[] = "0123456789ABCDEF";

void serial_hex32(uint32_t val)
{
    serial_putc('0');
    serial_putc('x');
    for (int i = 28; i >= 0; i -= 4)
        serial_putc(_hex[(val >> i) & 0xF]);
}
