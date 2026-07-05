#include <string.h>
#include "esp_wifi.h"
#include "esp_wifi_types.h"
#include "nvs_flash.h"
#include "csi_shared.h"
#include "../include/dso_config.h"
#include "../include/dso.h"
#include "serial.h"

#define WIFI_SSID CONFIG_DSOTRON_WIFI_SSID
#define WIFI_PASS CONFIG_DSOTRON_WIFI_PASS

static void csi_callback(void *ctx, wifi_csi_info_t *data)
{
    csi_sample_t s;
    int start = data->first_word_invalid ? 1 : 0;
    int n = (data->len / 2) - start;
    if (n > 64) n = 64;

    for (int i = 0; i < n; i++) {
        int I = data->buf[(i + start) * 2];
        int Q = data->buf[(i + start) * 2 + 1];
        int val = I + Q;
        if (val < 0) val = -val;
        s.amp[i] = (val > 127) ? 127 : (int8_t)val;
    }
    for (int i = n; i < 64; i++) s.amp[i] = 0;
    s.valid = 1;

    dso_fifo_push(&_csi_fifo, &s);
}

void csi_capture_init(void)
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        nvs_flash_erase();
        nvs_flash_init();
    }

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = WIFI_SSID,
            .password = WIFI_PASS,
            .threshold.authmode = WIFI_AUTH_WPA2_PSK,
        },
    };
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_ERROR_CHECK(esp_wifi_connect());

    wifi_csi_config_t csi_config = {
        .lltf_en = 1,
        .htltf_en = 1,
        .stbc_htltf2_en = 0,
        .ltf_merge_en = 1,
        .channel_filter_en = 0,
        .manual_scale = 0,
    };
    ESP_ERROR_CHECK(esp_wifi_set_csi_config(&csi_config));
    ESP_ERROR_CHECK(esp_wifi_set_csi_rx_cb(&csi_callback, NULL));
    ESP_ERROR_CHECK(esp_wifi_set_csi(1));

    serial_puts("WiFi STA + CSI enabled\n");
}
