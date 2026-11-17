/*
 * sched_check — проверка выполнимости статического расписания DSO-TRON.
 *
 * Планировщик принимает любую таблицу слотов, но не проверяет, что она вообще выполнима:
 * при перегрузке задачи будут молча копиться. Эта утилита считает:
 *
 *   • загрузку процессора U = Σ C_i / T_i;
 *   • время отклика R_i для каждой задачи методом последовательного приближения
 *     (response-time analysis для фиксированных приоритетов):
 *
 *         R_i = C_i + Σ_{j ∈ hp(i)} ceil(R_i / T_j) · C_j
 *
 *     где hp(i) — задачи, объявленные выше (порядок в файле = порядок приоритетов).
 *
 * Итерация монотонна и сходится для U < 1; чтобы не зависнуть при перегрузке, число итераций
 * ограничено, а выход за дедлайн прекращает расчёт сразу.
 *
 * Формат файла расписания (по одной задаче в строке, `#` — комментарий):
 *
 *   # id  period  offset  wcet_us  deadline_us
 *   1      10      0       800      10000
 *   2      10      5       700      10000
 *   3      50      0       4000     50000
 *
 * Код возврата: 0 — расписание выполнимо, 1 — нет, 2 — ошибка входных данных.
 */
#pragma once

#include <stdint.h>
#include <stdio.h>

#define SC_MAX_TASKS     16   /* столько же, сколько DSO_MAX_SLOTS */
#define SC_MAX_ITER      1000 /* предел итераций R_i: защита от нерасходящегося цикла */

typedef struct {
    int      id;
    uint32_t period;     /* T, тики */
    uint32_t offset;     /* фаза, тики */
    uint32_t wcet;       /* C, микросекунды */
    uint32_t deadline;   /* D, микросекунды (0 → принять равным периоду в мкс) */
} sc_task;

typedef struct {
    int      feasible;             /* 1 — все дедлайны выполняются и U < 1 */
    double   utilization;          /* Σ C/T */
    double   response[SC_MAX_TASKS];
    int      response_known[SC_MAX_TASKS];
    int      failed_index;         /* индекс первой задачи, сорвавшей дедлайн, или -1 */
    int      iterations;           /* сколько итераций понадобилось всего (для диагностики) */
} sc_result;

/* Разбирает одну строку. Возвращает 1 — разобрана, 0 — пустая/комментарий, -1 — ошибка. */
int sc_parse_line(const char *line, sc_task *out);

/* Читает файл. Возвращает число задач (>=0) или -1 при ошибке открытия/разбора. */
int sc_load_file(const char *path, sc_task *tasks, int max_tasks);

/* Анализ расписания. tasks[0] — самый высокий приоритет. Возвращает 0 при успехе. */
int sc_analyze(const sc_task *tasks, int n, double tick_hz, sc_result *out);

/* Печатает таблицу результатов в человекочитаемом виде. */
void sc_print(const sc_task *tasks, int n, const sc_result *res, double tick_hz, FILE *out);
