#include <stdio.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_now.h"
#include "esp_http_server.h"
#include "esp_vfs_fat.h"
#include "sdmmc_cmd.h"
#include "driver/i2c.h"
#include "driver/uart.h"
#include "config.h"

static const char *TAG = "VOSTOK_BASE";
static httpd_handle_t server = NULL;

typedef struct {
    uint8_t rod_id;
    float accel_x;
    float accel_y;
    float accel_z;
    uint16_t hall_val;
    uint32_t uptime;
} rod_data_t;

// WebSocket Broadcast
static void ws_broadcast(const char* data) {
    if (!server) return;
    size_t clients = 4;
    int client_fds[4];
    if (httpd_get_client_list(server, &clients, client_fds) == ESP_OK) {
        for (int i = 0; i < clients; i++) {
            if (httpd_ws_get_fd_info(server, client_fds[i]) == HTTPD_WS_CLIENT_WEBSOCKET) {
                httpd_ws_frame_t ws_pkt;
                memset(&ws_pkt, 0, sizeof(httpd_ws_frame_t));
                ws_pkt.payload = (uint8_t*)data;
                ws_pkt.len = strlen(data);
                ws_pkt.type = HTTPD_WS_TYPE_TEXT;
                httpd_ws_send_frame_async(server, client_fds[i], &ws_pkt);
            }
        }
    }
}

static void esp_now_recv_cb(const esp_now_recv_info_t *recv_info, const uint8_t *data, int len) {
    if (len == sizeof(rod_data_t)) {
        rod_data_t *msg = (rod_data_t *)data;
        char json_buf[128];
        snprintf(json_buf, sizeof(json_buf),
            "{\"type\":\"rod\",\"id\":%d,\"ax\":%.2f,\"ay\":%.2f,\"az\":%.2f,\"h\":%d}",
            msg->rod_id, msg->accel_x, msg->accel_y, msg->accel_z, msg->hall_val);
        ESP_LOGI(TAG, "Recv: %s", json_buf);
        ws_broadcast(json_buf);
    }
}

// HTTP Handlers
static esp_err_t ws_handler(httpd_req_t *req) {
    if (req->method == HTTP_GET) {
        ESP_LOGI(TAG, "Handshake done, the new connection was opened");
        return ESP_OK;
    }
    return ESP_OK;
}

static const httpd_uri_t ws = {
    .uri       = "/ws",
    .method    = HTTP_GET,
    .handler   = ws_handler,
    .user_ctx  = NULL,
    .is_websocket = true
};

void start_webserver() {
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port = WS_PORT;
    if (httpd_start(&server, &config) == ESP_OK) {
        httpd_register_uri_handler(server, &ws);
    }
}

void init_wifi() {
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_ap();
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    wifi_config_t wifi_config = {
        .ap = {
            .ssid = WIFI_SSID,
            .ssid_len = strlen(WIFI_SSID),
            .password = WIFI_PASS,
            .max_connection = 4,
            .authmode = WIFI_AUTH_WPA2_PSK
        },
    };
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());
}

void app_main(void) {
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
      ESP_ERROR_CHECK(nvs_flash_erase());
      ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    init_wifi();
    ESP_ERROR_CHECK(esp_now_init());
    ESP_ERROR_CHECK(esp_now_register_recv_cb(esp_now_recv_cb));

    start_webserver();

    ESP_LOGI(TAG, "VOSTOK NEXUS v5.1 S3 Base Station Active");

    while(1) {
        // Periodic sensor readings (simulated for now)
        char meteo_json[128];
        snprintf(meteo_json, sizeof(meteo_json),
            "{\"type\":\"meteo\",\"t\":24.5,\"p\":1013.2,\"h\":55}");
        ws_broadcast(meteo_json);
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}
