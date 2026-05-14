#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_http_server.h"
#include "esp_heap_caps.h"
#include "cJSON.h"
#include "config.h"
#include "nvs_handler.h"
#include "esp_now_logic.h"

static const char *TAG = "VOSTOK_MAIN";
httpd_handle_t server = NULL;
int client_fd = -1;
QueueHandle_t log_queue;

/* Vertical Profile Ring Buffer (PSRAM) */
typedef struct {
    float *data;
    size_t size;
    size_t head;
} ring_buffer_t;

ring_buffer_t* profile_buffer;

void init_profile_buffer(size_t points) {
    profile_buffer = heap_caps_malloc(sizeof(ring_buffer_t), MALLOC_CAP_SPIRAM);
    profile_buffer->data = heap_caps_malloc(points * sizeof(float), MALLOC_CAP_SPIRAM);
    profile_buffer->size = points;
    profile_buffer->head = 0;
}

/* Core 0 Tasks: High Priority Telemetry & Sensors */
void telemetry_task(void *pvParameters) {
    ESP_LOGI(TAG, "Starting Telemetry Task on Core 0");
    espnow_init_base();

    while(1) {
        // High frequency sensor polling/processing here if needed
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

/* Core 1 Tasks: Networking & UI Data Prep */
static esp_err_t ws_handler(httpd_req_t *req) {
    if (req->method == HTTP_GET) {
        client_fd = httpd_req_to_sockfd(req);
        ESP_LOGI(TAG, "WebSocket connected: %d", client_fd);
        return ESP_OK;
    }
    return ESP_OK;
}

static const httpd_uri_t ws_uri = {
    .uri = "/ws", .method = HTTP_GET, .handler = ws_handler, .is_websocket = true
};

void network_task(void *pvParameters) {
    ESP_LOGI(TAG, "Starting Network Task on Core 1");

    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.core_id = 1;
    if (httpd_start(&server, &config) == ESP_OK) {
        httpd_register_uri_handler(server, &ws_uri);
    }

    while(1) {
        if (client_fd != -1) {
            cJSON *root = cJSON_CreateObject();
            cJSON_AddStringToObject(root, "type", "base_data");
            cJSON_AddNumberToObject(root, "temp", 24.5 + (rand()%10)/10.0);

            char *json_str = cJSON_PrintUnformatted(root);
            httpd_ws_frame_t ws_pkt = { .payload = (uint8_t*)json_str, .len = strlen(json_str), .type = HTTPD_WS_TYPE_TEXT };
            httpd_ws_send_frame_async(server, client_fd, &ws_pkt);
            free(json_str);
            cJSON_Delete(root);
        }
        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}

void app_main(void) {
    ESP_LOGI(TAG, "Vostok Nexus v5.1 ULTRA PREMIUM Initializing...");

    nvs_init_storage();
    init_profile_buffer(100); // 100 points for meteo profile

    esp_netif_init();
    esp_event_loop_create_default();
    esp_netif_create_default_wifi_ap();
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&cfg);
    esp_wifi_set_mode(WIFI_MODE_AP);
    esp_wifi_start();

    log_queue = xQueueCreate(10, sizeof(log_msg_t));

    // Dual Core Task Pinning (Based on Context7 v5.x guidelines)
    xTaskCreatePinnedToCore(telemetry_task, "telemetry_task", 4096, NULL, 10, NULL, 0);
    xTaskCreatePinnedToCore(network_task, "network_task", 4096, NULL, 5, NULL, 1);
}
