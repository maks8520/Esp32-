#include <stdio.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_now.h"
#include "esp_http_server.h"
#include "esp_vfs_fat.h"
#include "sdmmc_cmd.h"
#include "driver/i2c.h"
#include "driver/uart.h"
#include "driver/gpio.h"
#include "esp_heap_caps.h"
#include "cJSON.h"
#include "config.h"

static const char *TAG = "VOSTOK_BASE";
static httpd_handle_t server = NULL;
static int client_fd = -1;
static QueueHandle_t log_queue;
static QueueHandle_t espnow_queue;

typedef struct {
    uint8_t mac[6];
    uint8_t data[sizeof(rod_data_t)];
    int len;
} espnow_event_t;

/* Utility: Fletcher-32 Checksum */
uint32_t calculate_checksum(const char *data, size_t len) {
    uint32_t sum1 = 0xffff, sum2 = 0xffff;
    while (len) {
        size_t tlen = len > 359 ? 359 : len;
        len -= tlen;
        do {
            sum1 += *data++;
            sum2 += sum1;
        } while (--tlen);
        sum1 = (sum1 & 0xffff) + (sum1 >> 16);
        sum2 = (sum2 & 0xffff) + (sum2 >> 16);
    }
    sum1 = (sum1 & 0xffff) + (sum1 >> 16);
    sum2 = (sum2 & 0xffff) + (sum2 >> 16);
    return (sum2 << 16) | sum1;
}

/* NVS Management */
esp_err_t get_windy_key(char *key_out, size_t max_len) {
    nvs_handle_t my_handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &my_handle);
    if (err != ESP_OK) return err;
    err = nvs_get_str(my_handle, NVS_KEY_WINDY, key_out, &max_len);
    nvs_close(my_handle);
    return err;
}

/* SD Card Logging Task (Core 0) */
void sd_log_task(void *pvParameters) {
    log_msg_t msg;
    while (1) {
        if (xQueueReceive(log_queue, &msg, portMAX_DELAY)) {
            FILE* f = fopen("/sdcard/telemetry.jsonl", "a");
            if (f) {
                // Sentinel Directive: SD card uses JSONL format with checksums
                fprintf(f, "{\"data\":%s,\"checksum\":%u}\n", msg.data, msg.checksum);
                fclose(f);
            }
        }
    }
}

/* ESP-NOW Handler Task (Core 1 - Bolt Directive: Sensor Fusion and ESP-NOW pinned to Core 1) */
void espnow_process_task(void *pvParameters) {
    espnow_event_t evt;
    while (1) {
        if (xQueueReceive(espnow_queue, &evt, portMAX_DELAY)) {
            if (evt.len == sizeof(rod_data_t)) {
                rod_data_t rod;
                memcpy(&rod, evt.data, evt.len);

                cJSON *root = cJSON_CreateObject();
                cJSON_AddStringToObject(root, "type", "rod_data");
                cJSON_AddNumberToObject(root, "id", rod.rod_id);
                cJSON_AddNumberToObject(root, "ax", rod.accel_x);
                cJSON_AddNumberToObject(root, "ay", rod.accel_y);
                cJSON_AddNumberToObject(root, "az", rod.accel_z);
                cJSON_AddNumberToObject(root, "bite", rod.bite_intensity);

                char *json_str = cJSON_PrintUnformatted(root);

                // Push to log queue (Core 0)
                log_msg_t log_msg;
                strncpy(log_msg.data, json_str, sizeof(log_msg.data) - 1);
                log_msg.checksum = calculate_checksum(json_str, strlen(json_str));
                xQueueSend(log_queue, &log_msg, 0);

                // Broadcast to WebSocket (Core 0 handled via server)
                if (client_fd != -1) {
                    httpd_ws_frame_t ws_pkt = { .payload = (uint8_t*)json_str, .len = strlen(json_str), .type = HTTPD_WS_TYPE_TEXT };
                    httpd_ws_send_frame_async(server, client_fd, &ws_pkt);
                }

                cJSON_Delete(root);
                free(json_str);
            }
        }
    }
}

/* ESP-NOW Receive Callback (Runs in WiFi context) */
static void espnow_recv_cb(const esp_now_recv_info_t *recv_info, const uint8_t *data, int len) {
    espnow_event_t evt;
    memcpy(evt.mac, recv_info->src_addr, 6);
    memcpy(evt.data, data, len > sizeof(evt.data) ? sizeof(evt.data) : len);
    evt.len = len;
    xQueueSend(espnow_queue, &evt, 0);
}

/* Web Server and WebSocket Handlers (Core 0) */
static esp_err_t ws_handler(httpd_req_t *req) {
    if (req->method == HTTP_GET) {
        client_fd = httpd_req_to_sockfd(req);
        char windy_key[64] = {0};
        if (get_windy_key(windy_key, sizeof(windy_key)) == ESP_OK) {
            cJSON *root = cJSON_CreateObject();
            cJSON_AddStringToObject(root, "type", "config");
            cJSON_AddStringToObject(root, "windy_key", windy_key);
            char *json_str = cJSON_PrintUnformatted(root);
            httpd_ws_frame_t ws_pkt = { .payload = (uint8_t*)json_str, .len = strlen(json_str), .type = HTTPD_WS_TYPE_TEXT };
            httpd_ws_send_frame_async(server, client_fd, &ws_pkt);
            free(json_str);
            cJSON_Delete(root);
        }
        return ESP_OK;
    }
    return ESP_OK;
}

static const httpd_uri_t ws_uri = { .uri = "/ws", .method = HTTP_GET, .handler = ws_handler, .is_websocket = true };

void server_task(void *pvParameters) {
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.task_priority = 5;
    config.core_id = 0;
    if (httpd_start(&server, &config) == ESP_OK) {
        httpd_register_uri_handler(server, &ws_uri);
    }
    while(1) { vTaskDelay(pdMS_TO_TICKS(1000)); }
}

/* Sensor Fusion Task (Core 1) */
void sensor_fusion_task(void *pvParameters) {
    // Bolt Directive: Use MALLOC_CAP_SPIRAM for large buffers
    float *weather_profile = heap_caps_malloc(5000 * sizeof(float), MALLOC_CAP_SPIRAM);

    while(1) {
        if (client_fd != -1) {
            cJSON *root = cJSON_CreateObject();
            cJSON_AddStringToObject(root, "type", "base_data");
            cJSON_AddNumberToObject(root, "temp", 24.2 + (rand() % 100 / 100.0));
            cJSON_AddNumberToObject(root, "press", 1013.2 + (rand() % 200 / 100.0));

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
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    esp_netif_init();
    esp_event_loop_create_default();
    esp_netif_create_default_wifi_ap();
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&cfg);
    esp_wifi_set_mode(WIFI_MODE_AP);
    esp_wifi_start();

    esp_now_init();
    esp_now_register_recv_cb(espnow_recv_cb);

    log_queue = xQueueCreate(LOG_QUEUE_SIZE, sizeof(log_msg_t));
    espnow_queue = xQueueCreate(20, sizeof(espnow_event_t));

    // Bolt Directive: Task pinning
    xTaskCreatePinnedToCore(server_task, "server_task", 4096, NULL, 5, NULL, 0);
    xTaskCreatePinnedToCore(sd_log_task, "sd_log_task", 4096, NULL, 4, NULL, 0);
    xTaskCreatePinnedToCore(espnow_process_task, "espnow_task", 4096, NULL, 6, NULL, 1);
    xTaskCreatePinnedToCore(sensor_fusion_task, "sensor_task", 4096, NULL, 5, NULL, 1);
}
