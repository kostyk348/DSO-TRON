/*
 * Тесты sched_check: разбор файла и анализ выполнимости.
 *
 * Проверяются ровно те утверждения, на которые опирается утилита:
 *   • загрузка U считается как Σ C/T;
 *   • R для одной задачи равен её WCET;
 *   • при перегрузке вердикт «не выполнимо» и расчёт завершается быстро (нет зависания);
 *   • при сорванном дедлайне указывается именно та задача, которая его сорвала;
 *   • мусор во входных данных отвергается с кодом ошибки.
 */
#include <stdio.h>
#include <string.h>

#include "sched_check.h"

static int failures;

#define CHECK(cond, msg)                                                        \
    do {                                                                        \
        if (!(cond)) {                                                          \
            printf("  FAIL  %s  (%s:%d)\n", (msg), __FILE__, __LINE__);         \
            failures++;                                                         \
        }                                                                       \
    } while (0)

#define CHECK_NEAR(actual, expected, tol, msg)                                  \
    do {                                                                        \
        double _a = (actual), _e = (expected);                                  \
        if ((_a - _e) > (tol) || (_e - _a) > (tol)) {                            \
            printf("  FAIL  %s: %.4f != %.4f (допуск %g)  (%s:%d)\n",            \
                   (msg), _a, _e, (double)(tol), __FILE__, __LINE__);           \
            failures++;                                                         \
        }                                                                       \
    } while (0)

/* 1 тик = 1 мкс: tick_hz = 1e6, тогда period в тиках == период в мкс — удобно для тестов */
static const double US_PER_TICK = 1e6;

static void test_parse_line(void)
{
    sc_task t;
    CHECK(sc_parse_line("# комментарий", &t) == 0, "комментарий пропускается");
    CHECK(sc_parse_line("   \n", &t) == 0, "пустая строка пропускается");
    CHECK(sc_parse_line("1 10 0 800 10000\n", &t) == 1, "корректная строка");
    CHECK(t.id == 1 && t.period == 10 && t.offset == 0 && t.wcet == 800 && t.deadline == 10000,
          "поля разобраны верно");

    CHECK(sc_parse_line("1 10 0 800\n", &t) == -1, "нехватка полей — ошибка");
    CHECK(sc_parse_line("0 10 0 800 10000", &t) == -1, "id 0 — ошибка");
    CHECK(sc_parse_line("1 0 0 800 10000", &t) == -1, "нулевой период — ошибка");
    CHECK(sc_parse_line("1 10 10 800 10000", &t) == -1, "смещение за периодом — ошибка");
    CHECK(sc_parse_line("1 10 0 0 10000", &t) == -1, "нулевая WCET — ошибка");
    CHECK(sc_parse_line("-1 10 0 800 10000", &t) == -1, "отрицательный id — ошибка");
}

static void test_single_task_response_is_wcet(void)
{
    sc_task t = {1, 10, 0, 1, 10};   /* T = 10 мкс, C = 1 мкс, U = 0.1 */
    sc_result res;
    CHECK(sc_analyze(&t, 1, US_PER_TICK, &res) == 0, "анализ одной задачи");
    CHECK(res.feasible, "одна задача с запасом выполнима");
    CHECK_NEAR(res.response[0], 1.0, 1e-6, "R одной задачи = её WCET");
    CHECK_NEAR(res.utilization, 0.1, 1e-6, "загрузка = C/T");
}

static void test_harmonic_set(void)
{
    /* T = 10/20/40 мкс, C = 1/2/4 мкс: U = 0.1 + 0.1 + 0.1 = 0.3, всё выполнимо */
    sc_task tasks[3] = {
        {1, 10, 0, 1, 10},
        {2, 20, 0, 2, 20},
        {3, 40, 0, 4, 40},
    };
    sc_result res;
    CHECK(sc_analyze(tasks, 3, US_PER_TICK, &res) == 0, "анализ гармонического набора");
    CHECK(res.feasible, "гармонический набор выполним");
    CHECK_NEAR(res.utilization, 0.3, 1e-9, "U = 0.3");
    /* первая задача не испытывает помех, вторая — одну активацию первой и т.д. */
    CHECK_NEAR(res.response[0], 1.0, 1e-6, "R1 = C1");
    CHECK_NEAR(res.response[1], 3.0, 1e-6, "R2 = C2 + C1");
    CHECK_NEAR(res.response[2], 7.0, 1e-6, "R3 = C3 + C2 + C1");
}

static void test_overload_is_infeasible_and_fast(void)
{
    /* U = 3 × 0.6 = 1.8 > 1: перегрузка */
    sc_task tasks[3] = {
        {1, 10, 0, 6, 10},
        {2, 10, 0, 6, 10},
        {3, 10, 0, 6, 10},
    };
    sc_result res;
    CHECK(sc_analyze(tasks, 3, US_PER_TICK, &res) == 0, "анализ перегруженного набора");
    CHECK(!res.feasible, "перегрузка — не выполнимо");
    CHECK(res.utilization > 1.0, "U > 1");
    CHECK(res.iterations < SC_MAX_ITER, "расчёт не упирается в предел итераций (нет зависания)");
}

static void test_deadline_miss_names_the_task(void)
{
    /* вторая задача: C=5, T=10, D=6, помеха от первой (C=3, T=10) → R = 5 + 3 = 8 > 6 */
    sc_task tasks[2] = {
        {1, 10, 0, 3, 10},
        {2, 10, 0, 5, 6},
    };
    sc_result res;
    CHECK(sc_analyze(tasks, 2, US_PER_TICK, &res) == 0, "анализ набора с сорванным дедлайном");
    CHECK(!res.feasible, "дедлайн второй задачи сорван — набор не выполним");
    CHECK(res.failed_index == 1, "указана именно вторая задача");
    CHECK_NEAR(res.response[1], 8.0, 1e-6, "R2 = C2 + C1");
}

static void test_high_utilization_within_deadlines(void)
{
    /* U = 0.9, но дедлайны с запасом: выполнимо */
    sc_task tasks[2] = {
        {1, 10, 0, 5, 10},
        {2, 100, 0, 40, 100},
    };
    sc_result res;
    CHECK(sc_analyze(tasks, 2, US_PER_TICK, &res) == 0, "анализ набора с U = 0.9");
    CHECK_NEAR(res.utilization, 0.9, 1e-9, "U = 0.9");
    CHECK(res.feasible, "при U = 0.9 и запасе по дедлайнам набор выполним");
}

static void test_bad_arguments(void)
{
    sc_result res;
    sc_task t = {1, 10, 0, 1, 10};
    CHECK(sc_analyze(NULL, 1, 100.0, &res) == -1, "NULL-массив — ошибка");
    CHECK(sc_analyze(&t, 0, 100.0, &res) == -1, "ноль задач — ошибка");
    CHECK(sc_analyze(&t, 1, 100.0, NULL) == -1, "NULL-результат — ошибка");
}

int main(void)
{
    printf("sched_check tests\n");

    test_parse_line();
    test_single_task_response_is_wcet();
    test_harmonic_set();
    test_overload_is_infeasible_and_fast();
    test_deadline_miss_names_the_task();
    test_high_utilization_within_deadlines();
    test_bad_arguments();

    if (failures == 0) {
        printf("OK: все проверки пройдены\n");
        return 0;
    }
    printf("ПРОВАЛЕНО проверок: %d\n", failures);
    return 1;
}
