/*
 * DSO-TRON Pipeline Demo
 * =======================
 *
 * Демонстрирует 3-stage pipeline с Time-Triggered планировщиком
 * и lock-free SPSC FIFO для межзадачного обмена.
 *
 * Pipeline:
 *   Sensor (T=50ms, off=0) → FIFO1 → Controller (T=50ms, off=2) → FIFO2 → Actuator (T=50ms, off=4)
 *
 * DSO-принципы:
 *   - Все структуры статические (TCB pool, stack arena, FIFO буферы)
 *   - Lock-free IPC: ни одного мьютекса, спинлока или disable_irq
 *   - Time-Triggered: детерминированное расписание, 0 джиттера
 *   - Единственный источник прерываний — PIT (100 Hz)
 */

#include "kernel.h"
#include "../include/dso.h"
#include "../include/dso_config.h"
#include "serial.h"

/* ── External declarations ─────────────────────────────── */
void idt_init(void);
void pic_init(void);
void pit_init(void);
void pit_unmask(void);
void vga_init(void);
void vga_puts(const char *s);
void vga_hex32(uint32_t val);

/* ── DSO Data Queues (lock-free SPSC FIFOs) ─────────────── */
DSO_FIFO_DECL(_fifo_sensor, uint32_t, 8);
DSO_FIFO_DECL(_fifo_control, uint32_t, 8);

/* ── Pipeline Task 1: Sensor ────────────────────────────── */
static uint32_t _sensor_val;

static void task_sensor(void)
{
    serial_puts(" S:");
    serial_hex32(_sensor_val);
    dso_fifo_push(&_fifo_sensor, &_sensor_val);
    _sensor_val++;
}

/* ── Pipeline Task 2: Controller ────────────────────────── */
static void task_control(void)
{
    uint32_t val;
    if (dso_fifo_pop(&_fifo_sensor, &val) == 0) {
        val *= 2;
        serial_puts(" C:");
        serial_hex32(val);
        dso_fifo_push(&_fifo_control, &val);
    }
}

/* ── Pipeline Task 3: Actuator ──────────────────────────── */
static void task_actuator(void)
{
    uint32_t val;
    if (dso_fifo_pop(&_fifo_control, &val) == 0) {
        serial_puts(" A:");
        serial_hex32(val);
    }
}

/* ── Time-Triggered Schedule ────────────────────────────── */
static const dso_slot _demo_schedule[] = {
    DSO_SLOT(1, 0, 5, 0, 500, task_sensor),
    DSO_SLOT(2, 1, 5, 2, 500, task_control),
    DSO_SLOT(3, 2, 5, 4, 500, task_actuator),
};

__attribute__((noreturn))
static void idle_loop(void)
{
    while (1) {
        __asm__ volatile("hlt" ::: "memory");
    }
}

void kernel_main(uint32_t mb_magic, uint32_t mb_info)
{
    (void)mb_info;

    serial_init();
    vga_init();
    vga_puts("DSO-TRON Pipeline Demo v0.1\n");
    serial_puts("DSO-TRON Pipeline Demo\n");
    serial_puts("magic: ");
    serial_hex32(mb_magic);
    serial_putc('\n');

    idt_init();
    pic_init();
    pit_init();
    vga_puts("Hardware init ok\n");
    serial_puts("HW init\n");

    dso_kernel_init();
    vga_puts("DSO kernel init ok\n");
    serial_puts("Kernel init\n");

    /* Create tasks */
    uint8_t _stack_a[2048] __attribute__((aligned(16)));
    uint8_t _stack_b[2048] __attribute__((aligned(16)));
    uint8_t _stack_c[2048] __attribute__((aligned(16)));

    T_CTSK ctsk_a = { TA_HLNG, 0, (FP*)task_sensor,    1, 2048, (VP)(uintptr_t)_stack_a };
    T_CTSK ctsk_b = { TA_HLNG, 0, (FP*)task_control,   1, 2048, (VP)(uintptr_t)_stack_b };
    T_CTSK ctsk_c = { TA_HLNG, 0, (FP*)task_actuator,  1, 2048, (VP)(uintptr_t)_stack_c };

    cre_tsk(1, &ctsk_a);
    cre_tsk(2, &ctsk_b);
    cre_tsk(3, &ctsk_c);
    serial_puts("Tasks created\n");

    /* Create DTQs */
    T_CDTQ cdtq = { TA_TFIFO, 8, sizeof(uint32_t), 0 };
    cre_dtq(1, &cdtq);
    cre_dtq(2, &cdtq);
    serial_puts("DTQs created\n");

    /* Register TT schedule */
    dso_sched_register(_demo_schedule, 3);
    serial_puts("TT schedule\n");

    pit_unmask();
    __asm__ volatile("sti");
    serial_puts("Running:\n");

    idle_loop();
}
