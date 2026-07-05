/*
 * VGA Text Mode Output — DSO-TRON
 * =================================
 * Минимальный вывод на 80x25 VGA text mode для отладки.
 * Статический буфер — никакого malloc.
 */

#include <stdint.h>

#define VGA_BUF    ((volatile uint16_t*)0xB8000)
#define VGA_WIDTH  80
#define VGA_HEIGHT 25

static uint8_t _vga_color = 0x0F;  /* white on black */
static int _vga_row = 0;
static int _vga_col = 0;

void vga_init(void)
{
    _vga_row = 0;
    _vga_col = 0;
    _vga_color = 0x0F;

    /* Clear screen */
    for (int i = 0; i < VGA_WIDTH * VGA_HEIGHT; i++)
        VGA_BUF[i] = 0x0F00 | ' ';
}

static void vga_scroll(void)
{
    for (int y = 1; y < VGA_HEIGHT; y++)
        for (int x = 0; x < VGA_WIDTH; x++)
            VGA_BUF[(y-1)*VGA_WIDTH + x] = VGA_BUF[y*VGA_WIDTH + x];
    for (int x = 0; x < VGA_WIDTH; x++)
        VGA_BUF[(VGA_HEIGHT-1)*VGA_WIDTH + x] = _vga_color << 8 | ' ';
    _vga_row = VGA_HEIGHT - 1;
}

void vga_putc(char c)
{
    if (c == '\n') {
        _vga_col = 0;
        _vga_row++;
        if (_vga_row >= VGA_HEIGHT) vga_scroll();
        return;
    }
    if (_vga_col >= VGA_WIDTH || c == '\r') {
        _vga_col = 0;
    }
    VGA_BUF[_vga_row * VGA_WIDTH + _vga_col] = _vga_color << 8 | (uint8_t)c;
    _vga_col++;
}

void vga_puts(const char *s)
{
    while (*s) vga_putc(*s++);
}

void vga_hex32(uint32_t val)
{
    for (int i = 28; i >= 0; i -= 4) {
        uint8_t nib = (val >> i) & 0xF;
        vga_putc(nib < 10 ? '0' + nib : 'a' + nib - 10);
    }
}
