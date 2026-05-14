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
 * @brief Инициализация I2C мастера для S3 (SDA: 4, SCL: 5).
 */
static void i2c_master_init() {
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = PIN_I2C_SDA,
        .scl_io_num = PIN_I2C_SCL,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = 400000,
    };
    i2c_param_config(I2C_NUM_0, &conf);
    i2c_driver_install(I2C_NUM_0, conf.mode, 0, 0, 0);
}

/**
 * @brief Инициализация NVS и сохранение ключа Windy.
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

        log_msg_t log_msg;
        strncpy(log_msg.data, json_str, sizeof(log_msg.data) - 1);
        xQueueSend(log_queue, &log_msg, 0);

        if (client_fd != -1) {
            httpd_ws_frame_t ws_pkt = { .payload = (uint8_t*)json_str, .len = strlen(json_str), .type = HTTPD_WS_TYPE_TEXT };
            httpd_ws_send_frame_async(server, client_fd, &ws_pkt);
        }

        cJSON_Delete(root);
        free(json_str);
    }
}

static esp_err_t ws_handler(httpd_req_t *req) {
    if (req->method == HTTP_GET) {
        client_fd = httpd_req_to_sockfd(req);
        return ESP_OK;
    }
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
    /* 1. Инициализация хранилища и периферии */
    init_nvs_manager();
    i2c_master_init();
    log_queue = xQueueCreate(LOG_QUEUE_SIZE, sizeof(log_msg_t));

    /* 2. WiFi и ESP-NOW (Core 0) */
    esp_netif_init();
    esp_event_loop_create_default();
    esp_netif_create_default_wifi_ap();
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&cfg);
    esp_wifi_set_mode(WIFI_MODE_AP);
    esp_wifi_start();

    esp_now_init();
    esp_now_register_recv_cb(espnow_recv_cb);

    /* 3. Запуск задач на разных ядрах */
    xTaskCreatePinnedToCore(start_web_server, "web_server", 4096, NULL, 5, NULL, 1);
    xTaskCreatePinnedToCore(sd_logging_task, "sd_log", 4096, NULL, 4, NULL, 1);

    ESP_LOGI(TAG, "Система VOSTOK NEXUS v5.1 S3 инициализирована (I2C: 4,5).");
}
