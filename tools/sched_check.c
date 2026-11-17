/*
 * sched_check.c — реализация проверки выполнимости расписания.
 *
 * Сложность: O(n² · iter). n ≤ SC_MAX_TASKS (16), iter ≤ SC_MAX_ITER (1000), то есть верхняя
 * граница — 256 000 операций, время счёта предсказуемо и не зависит от входных данных.
 */
#include "sched_check.h"

#include <ctype.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

static int is_blank_or_comment(const char *line)
{
    while (*line && isspace((unsigned char)*line)) line++;
    return *line == '\0' || *line == '#';
}

int sc_parse_line(const char *line, sc_task *out)
{
    if (!line || !out) return -1;
    if (is_blank_or_comment(line)) return 0;

    long id, period, offset;
    unsigned long wcet, deadline;
    int consumed = 0;
    int matched = sscanf(line, "%ld %ld %ld %lu %lu%n",
                         &id, &period, &offset, &wcet, &deadline, &consumed);
    if (matched != 5) return -1;
    if (id <= 0 || period <= 0 || offset < 0) return -1;
    if (offset >= period) return -1;         /* фаза вне периода — почти наверняка опечатка */
    if (wcet == 0 || deadline == 0) return -1;
    if (wcet > period * 1000000UL) return -1; /* WCET в мкс не может быть больше периода */

    out->id = (int)id;
    out->period = (uint32_t)period;
    out->offset = (uint32_t)offset;
    out->wcet = (uint32_t)wcet;
    out->deadline = (uint32_t)deadline;
    return 1;
}

int sc_load_file(const char *path, sc_task *tasks, int max_tasks)
{
    if (!path || !tasks || max_tasks <= 0) return -1;
    FILE *f = fopen(path, "r");
    if (!f) return -1;

    char line[512];
    int n = 0;
    int rc = 0;
    while (fgets(line, sizeof line, f)) {
        sc_task t;
        int r = sc_parse_line(line, &t);
        if (r < 0) { rc = -1; break; }
        if (r == 0) continue;
        if (n >= max_tasks) { rc = -1; break; }   /* больше слотов, чем поддерживает ядро */
        tasks[n++] = t;
    }
    fclose(f);
    return rc < 0 ? -1 : n;
}

/* Период в микросекундах при тактовой частоте ядра. */
static double period_us(uint32_t period_ticks, double tick_hz)
{
    if (tick_hz <= 0.0) return (double)period_ticks;   /* 1 тик = 1 мкс по умолчанию */
    return (double)period_ticks * 1e6 / tick_hz;
}

int sc_analyze(const sc_task *tasks, int n, double tick_hz, sc_result *out)
{
    if (!tasks || !out || n <= 0 || n > SC_MAX_TASKS) return -1;

    memset(out, 0, sizeof *out);
    out->failed_index = -1;
    out->feasible = 1;

    for (int i = 0; i < n; i++) {
        out->response[i] = 0.0;
        out->response_known[i] = 0;
        out->utilization += (double)tasks[i].wcet / period_us(tasks[i].period, tick_hz);
    }

    for (int i = 0; i < n; i++) {
        const double T_i = period_us(tasks[i].period, tick_hz);
        const double D_i = tasks[i].deadline > 0 ? (double)tasks[i].deadline : T_i;
        const double C_i = (double)tasks[i].wcet;

        /* R берём с запасом: начальное приближение — собственная WCET, дальше монотонно растёт. */
        double R = C_i;
        int iter = 0;
        int converged = 0;

        while (iter < SC_MAX_ITER) {
            iter++;
            out->iterations++;
            double next = C_i;
            for (int j = 0; j < i; j++) {          /* более приоритетные */
                double T_j = period_us(tasks[j].period, tick_hz);
                if (T_j <= 0.0) continue;
                double interference = ceil(R / T_j) * (double)tasks[j].wcet;
                next += interference;
                if (next > D_i) break;             /* уже сорвали дедлайн — дальше считать незачем */
            }
            if (next > D_i) { R = next; break; }
            if (next <= R + 1e-9) { converged = 1; break; }
            R = next;
        }

        out->response[i] = R;
        out->response_known[i] = converged || R <= D_i;
        if (R > D_i || !out->response_known[i]) {
            if (out->failed_index < 0) out->failed_index = i;
            out->feasible = 0;
        }
    }

    if (out->utilization > 1.0 + 1e-12) out->feasible = 0;
    return 0;
}

void sc_print(const sc_task *tasks, int n, const sc_result *res, double tick_hz, FILE *out)
{
    if (!tasks || !res || !out) return;
    fprintf(out, "id  period(ms)  wcet(us)  deadline(us)   R(us)   verdict\n");
    fprintf(out, "--  ----------  --------  ------------   -----   -------\n");
    for (int i = 0; i < n; i++) {
        const double T_us = period_us(tasks[i].period, tick_hz);
        const double D_i = tasks[i].deadline > 0 ? (double)tasks[i].deadline : T_us;
        int ok = res->response_known[i] && res->response[i] <= D_i;
        fprintf(out, "%2d  %10.3f  %8u  %12.0f  %7.1f   %s\n",
                tasks[i].id, T_us / 1000.0, tasks[i].wcet, D_i, res->response[i],
                ok ? "ok" : "MISS");
    }
    fprintf(out, "\nзагрузка U = %.4f  (нужно < 1.0)\n", res->utilization);
    if (res->feasible) {
        fprintf(out, "вердикт: расписание выполнимо\n");
    } else {
        fprintf(out, "вердикт: НЕ выполнимо");
        if (res->failed_index >= 0) {
            fprintf(out, " (первая сорванная задача: id %d)", tasks[res->failed_index].id);
        }
        fprintf(out, "\n");
    }
}

#ifndef SC_NO_MAIN
int main(int argc, char **argv)
{
    if (argc < 2) {
        fprintf(stderr, "usage: %s <schedule.txt> [tick_hz, по умолчанию 100]\n", argv[0]);
        return 2;
    }
    double tick_hz = (argc > 2) ? atof(argv[2]) : 100.0;

    sc_task tasks[SC_MAX_TASKS];
    int n = sc_load_file(argv[1], tasks, SC_MAX_TASKS);
    if (n <= 0) {
        fprintf(stderr, "не удалось прочитать расписание из %s\n", argv[1]);
        return 2;
    }

    sc_result res;
    if (sc_analyze(tasks, n, tick_hz, &res) != 0) {
        fprintf(stderr, "ошибка анализа\n");
        return 2;
    }

    printf("DSO-TRON schedule check (%s, tick %g Hz)\n\n", argv[1], tick_hz);
    sc_print(tasks, n, &res, tick_hz, stdout);
    return res.feasible ? 0 : 1;
}
#endif
