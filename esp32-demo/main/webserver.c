#include <string.h>
#include "esp_http_server.h"
#include "csi_shared.h"
#include "serial.h"

extern dso_stats_t *dso_stats_get(void);
extern void chaos_toggle(void);
extern int  chaos_is_active(void);
extern const char index_html[];

static int _ws_fd = -1;

static esp_err_t root_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, index_html, strlen(index_html));
    return ESP_OK;
}

static esp_err_t ws_handler(httpd_req_t *req)
{
    if (req->method == HTTP_GET) {
        return ESP_OK;
    }

    httpd_ws_frame_t ws;
    memset(&ws, 0, sizeof(ws));
    ws.type = HTTPD_WS_TYPE_TEXT;
    ws.len  = 256;
    char buf[256];
    ws.payload = (uint8_t*)buf;

    esp_err_t ret = httpd_ws_recv_frame(req, &ws, 256);
    if (ret != ESP_OK) return ret;

    if (ws.type == HTTPD_WS_TYPE_TEXT && ws.len > 0) {
        buf[ws.len] = 0;
        if (strstr(buf, "\"c\":1")) {
            chaos_toggle();
        } else if (strstr(buf, "\"c\":0")) {
            chaos_toggle();
        }
    }

    _ws_fd = httpd_req_to_sockfd(req);
    return ESP_OK;
}

static void ws_broadcast_task(void *arg)
{
    (void)arg;
    char buf[1024];
    httpd_handle_t server = (httpd_handle_t)arg;

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(50));

        if (_ws_fd < 0) continue;

        dso_stats_t *st = dso_stats_get();

        int pos = snprintf(buf, sizeof(buf),
            "{\"t\":%lu,\"c\":%lu,\"s\":%ld,\"m\":%d,\"a\":[",
            (unsigned long)st->ticks,
            (unsigned long)st->csi_count,
            (long)st->signal_avg,
            st->motion);

        for (int i = 0; i < 64; i++)
            pos += snprintf(buf + pos, sizeof(buf) - pos,
                "%s%ld", i ? "," : "", (long)st->amp[i]);
        snprintf(buf + pos, sizeof(buf) - pos, "]}");

        httpd_ws_frame_t ws = {
            .type = HTTPD_WS_TYPE_TEXT,
            .payload = (uint8_t*)buf,
            .len = strlen(buf),
        };
        httpd_ws_send_frame_async(server, _ws_fd, &ws);
    }
}

void webserver_start(void)
{
    httpd_config_t cfg = HTTPD_DEFAULT_CONFIG();
    cfg.max_open_sockets = 4;
    cfg.lru_purge_enable = 1;

    httpd_handle_t server = NULL;
    if (httpd_start(&server, &cfg) != ESP_OK) {
        serial_puts("HTTP start failed\n");
        return;
    }

    httpd_uri_t uri_root = {
        .uri = "/",
        .method = HTTP_GET,
        .handler = root_handler,
    };
    httpd_register_uri_handler(server, &uri_root);

    httpd_uri_t uri_ws = {
        .uri = "/ws",
        .method = HTTP_GET,
        .handler = ws_handler,
        .is_websocket = true,
    };
    httpd_register_uri_handler(server, &uri_ws);

    xTaskCreatePinnedToCore(ws_broadcast_task, "ws_bc", 4096, server, 5, NULL, 1);
}
