/*
 * DSO-TRON — host-side determinism tests for the time-triggered scheduler.
 *
 * Зачем: планировщик (include/dso_sched.h) не зависит от железа, поэтому его поведение
 * можно проверять на обычном x86/ARM хосте, без ESP32 и без QEMU. Эти тесты фиксируют
 * контракт, на который опирается всё остальное ядро:
 *
 *   1) слот срабатывает строго по (tick % period == offset);
 *   2) два прогона одной и той же программы дают побайтово одинаковую трассу;
 *   3) порядок выполнения внутри тика определяется полем priority;
 *   4) слот с period == 0 не срабатывает и не роняет программу (деление на ноль);
 *   5) за тик задача не запускается дважды;
 *   6) ready_ids не переполняется при любом числе слотов.
 *
 * Сборка: cmake -S . -B build-host && cmake --build build-host && ctest --test-dir build-host
 */
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "dso_sched.h"

static int failures;

#define CHECK(cond, msg)                                                        \
    do {                                                                        \
        if (!(cond)) {                                                          \
            printf("  FAIL  %s  (%s:%d)\n", (msg), __FILE__, __LINE__);         \
            failures++;                                                         \
        }                                                                       \
    } while (0)

/* ── трасса выполнения ─────────────────────────────────────────────── */

#define TRACE_MAX 4096
static uint32_t trace[TRACE_MAX];
static uint32_t trace_len;
static uint8_t  current_tick_of[16]; /* сколько раз задача запускалась в текущем тике */

static void tr_reset(void)
{
    trace_len = 0;
    memset(current_tick_of, 0, sizeof(current_tick_of));
}

static void tr_push(uint8_t id)
{
    if (trace_len < TRACE_MAX) trace[trace_len++] = id;
    if (id < sizeof(current_tick_of)) current_tick_of[id]++;
}

/* Обработчики задач: их id — то, что попадает в трассу. */
static void t1(void) { tr_push(1); }
static void t2(void) { tr_push(2); }
static void t3(void) { tr_push(3); }
static void t4(void) { tr_push(4); }
static void t5(void) { tr_push(5); }
static void t6(void) { tr_push(6); }
static void t7(void) { tr_push(7); }
static void t8(void) { tr_push(8); }

/* ── прогон расписания ─────────────────────────────────────────────── */

static uint8_t ready_scratch[32];

static void run_ticks(dso_scheduler *sched, uint32_t ticks)
{
    for (uint32_t t = 0; t < ticks; t++) {
        memset(current_tick_of, 0, sizeof(current_tick_of));
        uint32_t ready = dso_sched_tick(sched);
        uint32_t ran = 0;
        while (dso_sched_run_next(sched) == 0) ran++;
        if (ran != ready) {
            printf("  FAIL  выполнено %u из %u готовых задач\n", ran, ready);
            failures++;
        }
        for (uint32_t id = 1; id < sizeof(current_tick_of); id++) {
            if (current_tick_of[id] > 1) {
                printf("  FAIL  задача %u запущена %u раз за тик\n", id, current_tick_of[id]);
                failures++;
            }
        }
    }
}

static uint32_t count_of(uint32_t id)
{
    uint32_t n = 0;
    for (uint32_t i = 0; i < trace_len; i++) if (trace[i] == id) n++;
    return n;
}

/* ── тесты ─────────────────────────────────────────────────────────── */

static void test_period_and_offset(void)
{
    static const dso_slot slots[] = {
        DSO_SLOT(1, 0, 10, 0, 0, t1),   /* каждые 10 тиков с 0-го */
        DSO_SLOT(2, 1, 10, 5, 0, t2),   /* каждые 10 тиков со 5-го */
        DSO_SLOT(3, 2, 50, 0, 0, t3),   /* каждые 50 тиков */
    };
    dso_scheduler sched;
    tr_reset();
    dso_sched_init(&sched, slots, 3, ready_scratch);
    run_ticks(&sched, 1000);

    CHECK(count_of(1) == 100, "задача 1 должна сработать 100 раз за 1000 тиков");
    CHECK(count_of(2) == 100, "задача 2 должна сработать 100 раз за 1000 тиков");
    CHECK(count_of(3) == 20, "задача 3 должна сработать 20 раз за 1000 тиков");
    CHECK(sched.tick == 1000, "счётчик тиков должен дойти до 1000");
}

static void test_determinism(void)
{
    static const dso_slot slots[] = {
        DSO_SLOT(1, 2, 3, 0, 0, t1),
        DSO_SLOT(2, 0, 5, 1, 0, t2),
        DSO_SLOT(3, 1, 7, 2, 0, t3),
        DSO_SLOT(4, 0, 11, 3, 0, t4),
    };
    static uint32_t first[TRACE_MAX];
    uint32_t first_len;

    dso_scheduler a;
    tr_reset();
    dso_sched_init(&a, slots, 4, ready_scratch);
    run_ticks(&a, 300);
    memcpy(first, trace, trace_len * sizeof(uint32_t));
    first_len = trace_len;

    dso_scheduler b;
    tr_reset();
    dso_sched_init(&b, slots, 4, ready_scratch);
    run_ticks(&b, 300);

    CHECK(first_len > 0, "трасса не должна быть пустой");
    CHECK(trace_len == first_len, "длина трассы должна совпадать");
    CHECK(memcmp(first, trace, trace_len * sizeof(uint32_t)) == 0,
          "два прогона одного расписания обязаны дать одинаковую трассу");
}

static void test_priority_order_within_tick(void)
{
    /* слоты заданы «не по порядку» приоритетов: порядок в тике — 2, затем 1, затем 3 */
    static const dso_slot slots[] = {
        DSO_SLOT(1, 1, 1, 0, 0, t1),
        DSO_SLOT(2, 2, 1, 0, 0, t2),
        DSO_SLOT(3, 0, 1, 0, 0, t3),
    };
    dso_scheduler sched;
    tr_reset();
    dso_sched_init(&sched, slots, 3, ready_scratch);
    run_ticks(&sched, 1);

    CHECK(trace_len == 3, "в тике должно выполниться три задачи");
    CHECK(trace[0] == 3, "первой идёт задача с priority 0");
    CHECK(trace[1] == 1, "второй — с priority 1");
    CHECK(trace[2] == 2, "третьей — с priority 2");
}

static void test_zero_period_is_ignored(void)
{
    static const dso_slot slots[] = {
        DSO_SLOT(1, 0, 0, 0, 0, t1),   /* period 0 — слот выключен */
        DSO_SLOT(2, 0, 4, 0, 0, t2),
    };
    dso_scheduler sched;
    tr_reset();
    dso_sched_init(&sched, slots, 2, ready_scratch);
    run_ticks(&sched, 100); /* без guard цикл упал бы на tick % 0 */

    CHECK(count_of(1) == 0, "слот с period 0 не должен срабатывать");
    CHECK(count_of(2) == 25, "живой слот должен сработать 25 раз за 100 тиков");
}

static void test_no_double_launch_in_tick(void)
{
    static const dso_slot slots[] = {
        DSO_SLOT(1, 0, 1, 0, 0, t1),
        DSO_SLOT(2, 0, 1, 0, 0, t2),
        DSO_SLOT(3, 0, 2, 0, 0, t3),
        DSO_SLOT(4, 0, 3, 0, 0, t4),
    };
    dso_scheduler sched;
    tr_reset();
    dso_sched_init(&sched, slots, 4, ready_scratch);
    run_ticks(&sched, 60);
    /* run_ticks сам проверяет, что ни одна задача не запускается дважды за тик */
    CHECK(trace_len > 0, "трасса не должна быть пустой");
}

static void test_full_schedule_fits_scratch(void)
{
    static const dso_slot slots[] = {
        DSO_SLOT(1, 0, 1, 0, 0, t1), DSO_SLOT(2, 1, 1, 0, 0, t2),
        DSO_SLOT(3, 2, 1, 0, 0, t3), DSO_SLOT(4, 3, 1, 0, 0, t4),
        DSO_SLOT(5, 4, 1, 0, 0, t5), DSO_SLOT(6, 5, 1, 0, 0, t6),
        DSO_SLOT(7, 6, 1, 0, 0, t7), DSO_SLOT(8, 7, 1, 0, 0, t8),
    };
    dso_scheduler sched;
    tr_reset();
    dso_sched_init(&sched, slots, 8, ready_scratch);
    run_ticks(&sched, 10);

    CHECK(sched.num_ready == 8, "все восемь слотов готовы каждый тик");
    CHECK(trace_len == 80, "8 задач × 10 тиков = 80 запусков");
    CHECK(trace[0] == 1 && trace[7] == 8, "порядок при равных приоритетах — как в таблице слотов");
}

static void test_long_run_is_stable(void)
{
    static const dso_slot slots[] = {
        DSO_SLOT(1, 0, 2, 0, 0, t1),
        DSO_SLOT(2, 1, 3, 1, 0, t2),
        DSO_SLOT(3, 2, 5, 2, 0, t3),
        DSO_SLOT(4, 3, 7, 3, 0, t4),
        DSO_SLOT(5, 4, 11, 4, 0, t5),
        DSO_SLOT(6, 5, 13, 5, 0, t6),
    };
    dso_scheduler sched;
    tr_reset();
    dso_sched_init(&sched, slots, 6, ready_scratch);
    run_ticks(&sched, 20000);

    /* период 2 за 20000 тиков = 10000 срабатываний; проверяем то, что успело попасть в трассу */
    uint32_t n1 = count_of(1);
    CHECK(n1 > 0, "задача 1 должна срабатывать");
    CHECK(sched.num_ready <= 6, "готовых задач не больше числа слотов");
    CHECK(sched.tick == 20000, "длинный прогон доходит до конца без переполнений");
}

int main(void)
{
    printf("DSO-TRON host tests (scheduler)\n");

    test_period_and_offset();
    test_determinism();
    test_priority_order_within_tick();
    test_zero_period_is_ignored();
    test_no_double_launch_in_tick();
    test_full_schedule_fits_scratch();
    test_long_run_is_stable();

    if (failures == 0) {
        printf("OK: все проверки пройдены\n");
        return 0;
    }
    printf("ПРОВАЛЕНО проверок: %d\n", failures);
    return 1;
}
