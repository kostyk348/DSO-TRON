#include "sdkconfig.h"
#include "esp_attr.h"
#include "esp_timer.h"
#include "driver/gptimer.h"
#include "../include/dso.h"

static void IRAM_ATTR dso_tick_isr(void *arg)
{
    (void)arg;
    dso_tick_handler();
}

void dso_timer_init(void)
{
    const esp_timer_create_args_t args = {
        .callback = &dso_tick_isr,
        .name = "dso_tick",
        .dispatch_method = ESP_TIMER_ISR,
    };
    esp_timer_handle_t timer;
    esp_timer_create(&args, &timer);
    esp_timer_start_periodic(timer, 10000);
}
