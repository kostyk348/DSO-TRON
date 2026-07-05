#include <string.h>
#include "kernel.h"
#include "../include/dso.h"
#include "../include/dso_config.h"
#include "serial.h"

/* ── External API from modules ──────────────────────────── */
void dso_timer_init(void);
void csi_capture_init(void);
void webserver_start(void);
void csi_sample_push(int8_t *amp, int valid);

/* ── Shared state (Core 0 writes, Core 1 reads) ─────────── */
typedef struct {
    int32_t  amp[64];
    int      motion;
    uint32_t ticks;
    uint32_t csi_count;
    int32_t  signal_avg;
    int      chaos_mode;
} dso_stats_t;

static dso_stats_t _stats;
static uint32_t    _ticks;
static uint32_t    _csi_count;
static int32_t     _smooth[64];
static int         _motion;
static int         _chaos;

/* Chaos API */
int  chaos_is_active(void);
void chaos_toggle(void);

/* ── DSO TT Tasks ────────────────────────────────────────── */

static void task_csi_proc(void)
{
    csi_sample_t s;
    while (dso_fifo_pop(&_csi_fifo, &s) == 0) {
        if (s.valid) {
            for (int i = 0; i < 64; i++)
                _smooth[i] = (_smooth[i] * 7 + s.amp[i] * 3) / 10;
            _csi_count++;
        }
    }
    _ticks++;
}

static void task_detector(void)
{
    int32_t total = 0;
    for (int i = 0; i < 64; i++) total += _smooth[i];
    int32_t avg = total / 64;

    static int32_t baseline;
    int32_t delta = (baseline > 0) ? (avg - baseline) : 0;
    if (delta < 0) delta = -delta;

    if (baseline == 0) baseline = avg;
    baseline = (baseline * 63 + avg) / 64;

    _motion = (delta > 32) ? 1 : 0;
}

static void task_stats(void)
{
    int32_t total = 0;
    for (int i = 0; i < 64; i++) total += _smooth[i];

    _stats.ticks     = _ticks;
    _stats.csi_count = _csi_count;
    _stats.motion    = _motion;
    _stats.signal_avg = total / 64;
    for (int i = 0; i < 64; i++) _stats.amp[i] = _smooth[i];
}

static const dso_slot _schedule[] = {
    DSO_SLOT(1, 0, 10, 0, 500, task_csi_proc),
    DSO_SLOT(2, 1, 10, 5, 500, task_detector),
    DSO_SLOT(3, 2, 100, 0, 500, task_stats),
};

dso_stats_t *dso_stats_get(void) { return &_stats; }

void chaos_toggle(void)
{
    _chaos = !_chaos;
}

int chaos_is_active(void)
{
    return _chaos;
}

void app_main(void)
{
    serial_init();
    serial_puts("\nDSOTRON-CSI v0.1\n");

    dso_kernel_init();
    serial_puts("DSO init\n");

    dso_sched_register(_schedule, 3);
    serial_puts("TT schedule\n");

    /* WiFi + CSI */
    csi_capture_init();
    serial_puts("CSI started\n");

    /* HTTP + WebSocket (FreeRTOS task on Core 1) */
    webserver_start();
    serial_puts("Web server started\n");

    /* DSO 100Hz timer ISR on Core 0 */
    dso_timer_init();
    serial_puts("Timer 100Hz\n");

    serial_puts("Running.\n");
}
