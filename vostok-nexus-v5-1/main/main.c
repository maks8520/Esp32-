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
#include "driver/gpio.h"
#include "cJSON.h"
#include "config.h"

static const char *TAG = "VOSTOK_NEXUS_BASE";
static httpd_handle_t server = NULL;
static int client_fd = -1;

/* Component Stubs (In a real project, these would be separate components) */
typedef struct {
    float temp;
    float press;
    float hum;
} bme280_data_t;

void bme280_read(bme280_data_t *data) {
    // Stub for actual BME280 I2C reading
    data->temp = 24.2 + (rand() % 100 / 100.0);
    data->press = 1013.2 + (rand() % 200 / 100.0);
    data->hum = 45.0 + (rand() % 500 / 100.0);
}

void gps_process_task(void *pvParameters) {
    while(1) {
        // Read from UART_NUM_1 and parse NMEA
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

/* --- Peripherals --- */
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

static void sd_card_init() {
    esp_vfs_fat_sdmmc_mount_config_t mount_config = {
        .format_if_mount_failed = true,
        .max_files = 5,
        .allocation_unit_size = 16 * 1024
    };
    sdmmc_host_t host = SDSPI_HOST_DEFAULT();
    spi_bus_config_t bus_cfg = {
        .mosi_io_num = PIN_SD_MOSI,
        .miso_io_num = PIN_SD_MISO,
        .sclk_io_num = PIN_SD_CLK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 4000,
    };
    spi_bus_initialize(host.slot, &bus_cfg, SDSPI_DEFAULT_DMA_CHAN);
    sdspi_device_config_t slot_config = SDSPI_DEVICE_CONFIG_DEFAULT();
    slot_config.gpio_cs = PIN_SD_CS;
    slot_config.host_id = host.slot;

    esp_vfs_fat_sdspi_mount("/sdcard", &host, &slot_config, &mount_config, NULL);
}

/* --- Networking --- */
static void espnow_recv_cb(const esp_now_recv_info_t *recv_info, const uint8_t *data, int len) {
    if (len == sizeof(rod_data_t)) {
        rod_data_t rod;
        memcpy(&rod, data, len);

        cJSON *root = cJSON_CreateObject();
        cJSON_AddStringToObject(root, "type", "rod_data");
        cJSON_AddNumberToObject(root, "id", rod.rod_id);
        cJSON_AddNumberToObject(root, "ax", rod.accel_x);
        cJSON_AddNumberToObject(root, "ay", rod.accel_y);
        cJSON_AddNumberToObject(root, "az", rod.accel_z);
        cJSON_AddNumberToObject(root, "bite", rod.bite_intensity);

        char *json_str = cJSON_PrintUnformatted(root);

        // Log to SD
        FILE* f = fopen("/sdcard/telemetry.jsonl", "a");
        if (f) { fprintf(f, "%s\n", json_str); fclose(f); }

        // Broadcast to WebSocket
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
        ESP_LOGI(TAG, "New WebSocket client connected: %d", client_fd);
        return ESP_OK;
    }
    return ESP_OK;
}

static const httpd_uri_t ws = { .uri = "/ws", .method = HTTP_GET, .handler = ws_handler, .is_websocket = true };

static void start_webserver(void) {
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    if (httpd_start(&server, &config) == ESP_OK) {
        httpd_register_uri_handler(server, &ws);
    }
}

void app_main(void) {
    ESP_LOGI(TAG, "VOSTOK NEXUS v5.1 Base Initializing...");

    nvs_flash_init();
    esp_netif_init();
    esp_event_loop_create_default();
    esp_netif_create_default_wifi_ap();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&cfg);
    wifi_config_t wifi_config = {
        .ap = { .ssid = WIFI_SSID, .password = WIFI_PASS, .channel = 1, .max_connection = 4, .authmode = WIFI_AUTH_WPA_WPA2_PSK },
    };
    esp_wifi_set_mode(WIFI_MODE_AP);
    esp_wifi_set_config(WIFI_IF_AP, &wifi_config);
    esp_wifi_start();

    esp_now_init();
    esp_now_register_recv_cb(espnow_recv_cb);

    i2c_master_init();
    sd_card_init();
    start_webserver();
    xTaskCreate(gps_process_task, "gps_task", 4096, NULL, 5, NULL);

    while(1) {
        if (client_fd != -1) {
            bme280_data_t env;
            bme280_read(&env);

            cJSON *root = cJSON_CreateObject();
            cJSON_AddStringToObject(root, "type", "base_data");
            cJSON_AddNumberToObject(root, "temp", env.temp);
            cJSON_AddNumberToObject(root, "press", env.press);
            cJSON_AddNumberToObject(root, "hum", env.hum);

            char *json_str = cJSON_PrintUnformatted(root);
            httpd_ws_frame_t ws_pkt = { .payload = (uint8_t*)json_str, .len = strlen(json_str), .type = HTTPD_WS_TYPE_TEXT };
            httpd_ws_send_frame_async(server, client_fd, &ws_pkt);

            cJSON_Delete(root);
            free(json_str);
        }
        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}
