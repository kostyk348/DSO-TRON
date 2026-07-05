#include <stdlib.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "serial.h"

extern int chaos_is_active(void);

static void chaos_task(void *arg)
{
    (void)arg;
    serial_puts("Chaos task started\n");

    while (1) {
        if (!chaos_is_active()) {
            vTaskDelay(pdMS_TO_TICKS(100));
            continue;
        }

        /* malloc/free storm */
        for (int i = 0; i < 100; i++) {
            void *p = malloc(rand() % 512 + 16);
            if (p) {
                memset(p, 0xAA, 16);
                free(p);
            }
        }

        /* CPU burn */
        volatile int x = 0;
        for (int i = 0; i < 50000; i++) x += i * i;

        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

void chaos_init(void)
{
    xTaskCreatePinnedToCore(chaos_task, "chaos", 3072, NULL, 3, NULL, 1);
}
