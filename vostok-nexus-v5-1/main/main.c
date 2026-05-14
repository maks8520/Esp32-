#include <stdio.h>
#include <string.h>
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
#include "cJSON.h"
#include "config.h"

static const char *TAG = "VOSTOK_BASE";
static httpd_handle_t server = NULL;
static int client_fd = -1;
static QueueHandle_t log_queue;

/**
 * @brief Инициализация NVS и сохранение ключа Windy если его нет.
 */
void init_nvs_manager() {
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);

    nvs_handle_t handle;
    if (nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle) == ESP_OK) {
        char key[64];
        size_t size = sizeof(key);
        if (nvs_get_str(handle, NVS_KEY_WINDY, key, &size) != ESP_OK) {
            nvs_set_str(handle, NVS_KEY_WINDY, WINDY_API_KEY_DEF);
            nvs_commit(handle);
        }
        nvs_close(handle);
    }
}

/**
 * @brief Задача логирования на SD карту (Core 1).
 * Работает в неблокирующем режиме через очередь.
 */
void sd_logging_task(void *pvParameters) {
    log_msg_t msg;
    while (1) {
        if (xQueueReceive(log_queue, &msg, portMAX_DELAY)) {
            FILE* f = fopen("/sdcard/fishing.jsonl", "a");
            if (f) {
                fprintf(f, "%s\n", msg.data);
                fclose(f);
            }
        }
    }
}

/**
 * @brief Обработчик входящих данных ESP-NOW (Core 0).
 */
static void espnow_recv_cb(const esp_now_recv_info_t *recv_info, const uint8_t *data, int len) {
    if (len == sizeof(rod_data_t)) {
        rod_data_t rod;
        memcpy(&rod, data, len);

        cJSON *root = cJSON_CreateObject();
        cJSON_AddStringToObject(root, "type", "rod_data");
        cJSON_AddNumberToObject(root, "id", rod.rod_id);
        cJSON_AddNumberToObject(root, "bite", rod.bite_intensity);

        char *json_str = cJSON_PrintUnformatted(root);

        // Отправка в очередь логирования
        log_msg_t log_msg;
        strncpy(log_msg.data, json_str, sizeof(log_msg.data) - 1);
        xQueueSend(log_queue, &log_msg, 0);

        // Отправка в WebSocket
        if (client_fd != -1) {
            httpd_ws_frame_t ws_pkt = { .payload = (uint8_t*)json_str, .len = strlen(json_str), .type = HTTPD_WS_TYPE_TEXT };
            httpd_ws_send_frame_async(server, client_fd, &ws_pkt);
        }

        cJSON_Delete(root);
        free(json_str);
    }
}

/**
 * @brief WebSocket обработчик (Core 1).
 */
static esp_err_t ws_handler(httpd_req_t *req) {
    if (req->method == HTTP_GET) {
        client_fd = httpd_req_to_sockfd(req);
        return ESP_OK;
    }
    // Здесь можно добавить санитарную проверку входящих пакетов
    return ESP_OK;
}

static const httpd_uri_t ws = { .uri = "/ws", .method = HTTP_GET, .handler = ws_handler, .is_websocket = true };

void start_web_server() {
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.core_id = 1;
    if (httpd_start(&server, &config) == ESP_OK) {
        httpd_register_uri_handler(server, &ws);
    }
}

void app_main(void) {
    // 1. Инициализация хранилища
    init_nvs_manager();
    log_queue = xQueueCreate(LOG_QUEUE_SIZE, sizeof(log_msg_t));

    // 2. WiFi и ESP-NOW (Core 0 для прерываний и телеметрии)
    esp_netif_init();
    esp_event_loop_create_default();
    esp_netif_create_default_wifi_ap();
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&cfg);
    esp_wifi_set_mode(WIFI_MODE_AP);
    esp_wifi_start();

    esp_now_init();
    esp_now_register_recv_cb(espnow_recv_cb);

    // 3. Запуск сервисов на разных ядрах
    xTaskCreatePinnedToCore(start_web_server, "web_server", 4096, NULL, 5, NULL, 1);
    xTaskCreatePinnedToCore(sd_logging_task, "sd_log", 4096, NULL, 4, NULL, 1);

    ESP_LOGI(TAG, "Система VOSTOK NEXUS v5.1 запущена на двух ядрах.");
}
