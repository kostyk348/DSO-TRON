/*
 * IDT — DSO-TRON
 * ===============
 * Минимальная IDT: PIT timer (IRQ0 → INT 0x20), остальные заглушки.
 * Только timer прерывание размаскировано.
 */

#include <stdint.h>
#include <string.h>

void pit_handler(void *frame);  /* from pit.c */

typedef struct {
    uint16_t offset_low;
    uint16_t selector;
    uint8_t  ist;
    uint8_t  type_attr;
    uint16_t offset_mid;
    uint32_t offset_high;
    uint32_t reserved;
} __attribute__((packed)) idt_entry;

typedef struct {
    uint16_t limit;
    uint64_t base;
} __attribute__((packed)) idtr;

static idt_entry idt[256];

#define IDT_ENTRY(vec, fn, dpl) \
    do { \
        uint64_t addr = (uint64_t)(fn); \
        idt[vec].offset_low  = addr & 0xFFFF; \
        idt[vec].offset_mid  = (addr >> 16) & 0xFFFF; \
        idt[vec].offset_high = (addr >> 32) & 0xFFFFFFFF; \
        idt[vec].selector    = 0x08; \
        idt[vec].ist         = 0; \
        idt[vec].type_attr   = 0x8E | ((dpl) << 5); \
        idt[vec].reserved    = 0; \
    } while(0)

/* Default handler — hangs */
__attribute__((interrupt))
static void default_handler(void *frame)
{
    (void)frame;
    __asm__ volatile("cli; hlt" ::: "memory");
    while(1);
}

#define DEF_HANDLER(num) \
    __attribute__((interrupt)) static void def_h_##num(void *f) { (void)f; default_handler(f); }

/* Generate 48 default handlers for vectors 0-47 (CPU exceptions + PIC) */
DEF_HANDLER(0);  DEF_HANDLER(1);  DEF_HANDLER(2);  DEF_HANDLER(3);
DEF_HANDLER(4);  DEF_HANDLER(5);  DEF_HANDLER(6);  DEF_HANDLER(7);
DEF_HANDLER(8);  DEF_HANDLER(9);  DEF_HANDLER(10); DEF_HANDLER(11);
DEF_HANDLER(12); DEF_HANDLER(13); DEF_HANDLER(14); DEF_HANDLER(15);
DEF_HANDLER(16); DEF_HANDLER(17); DEF_HANDLER(18); DEF_HANDLER(19);
DEF_HANDLER(20); DEF_HANDLER(21); DEF_HANDLER(22); DEF_HANDLER(23);
DEF_HANDLER(24); DEF_HANDLER(25); DEF_HANDLER(26); DEF_HANDLER(27);
DEF_HANDLER(28); DEF_HANDLER(29); DEF_HANDLER(30); DEF_HANDLER(31);
DEF_HANDLER(32); DEF_HANDLER(33); DEF_HANDLER(34); DEF_HANDLER(35);
DEF_HANDLER(36); DEF_HANDLER(37); DEF_HANDLER(38); DEF_HANDLER(39);
DEF_HANDLER(40); DEF_HANDLER(41); DEF_HANDLER(42); DEF_HANDLER(43);
DEF_HANDLER(44); DEF_HANDLER(45); DEF_HANDLER(46); DEF_HANDLER(47);

static void *handlers[48] = {
    def_h_0, def_h_1, def_h_2, def_h_3, def_h_4, def_h_5, def_h_6, def_h_7,
    def_h_8, def_h_9, def_h_10, def_h_11, def_h_12, def_h_13, def_h_14, def_h_15,
    def_h_16, def_h_17, def_h_18, def_h_19, def_h_20, def_h_21, def_h_22, def_h_23,
    def_h_24, def_h_25, def_h_26, def_h_27, def_h_28, def_h_29, def_h_30, def_h_31,
    def_h_32, def_h_33, def_h_34, def_h_35, def_h_36, def_h_37, def_h_38, def_h_39,
    def_h_40, def_h_41, def_h_42, def_h_43, def_h_44, def_h_45, def_h_46, def_h_47,
};

void idt_init(void)
{
    /* Fill IDT with default handlers */
    for (int i = 0; i < 48; i++)
        IDT_ENTRY(i, handlers[i], 0);

    /* PIT timer → INT 0x20 */
    IDT_ENTRY(0x20, pit_handler, 0);

    /* Load IDT */
    idtr _idtr = { sizeof(idt) - 1, (uint64_t)&idt };
    __asm__ volatile("lidt %0" :: "m"(_idtr) : "memory");
}
