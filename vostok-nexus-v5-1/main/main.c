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
#include "driver/i2c.h"
#include "driver/uart.h"
#include "driver/gpio.h"
#include "nvs_flash.h"
#include "config.h"
#include "nvs_handler.h"
#include "esp_now_logic.h"

static const char *TAG = "VOSTOK_BASE";
httpd_handle_t server = NULL;
int client_fd = -1;
QueueHandle_t log_queue;

static void peripherals_init() {
    i2c_config_t i2c_conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = PIN_I2C_SDA,
        .scl_io_num = PIN_I2C_SCL,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = 100000,
    };
    i2c_param_config(I2C_NUM_0, &i2c_conf);
    i2c_driver_install(I2C_NUM_0, i2c_conf.mode, 0, 0, 0);

    uart_config_t uart_config = {
        .baud_rate = 9600,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
    };
    uart_param_config(UART_NUM_1, &uart_config);
    uart_set_pin(UART_NUM_1, PIN_GPS_TX, PIN_GPS_RX, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    uart_driver_install(UART_NUM_1, 1024, 0, 0, NULL, 0);
}

static esp_err_t ws_handler(httpd_req_t *req) {
    if (req->method == HTTP_GET) {
        client_fd = httpd_req_to_sockfd(req);
        ESP_LOGI(TAG, "WebSocket client connected");
        return ESP_OK;
    }
    return ESP_OK;
}

static const httpd_uri_t ws = { .uri = "/ws", .method = HTTP_GET, .handler = ws_handler, .is_websocket = true };

void network_task(void *pvParameters) {
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.core_id = 1;
    if (httpd_start(&server, &config) == ESP_OK) {
        httpd_register_uri_handler(server, &ws);
    }
    while(1) { vTaskDelay(pdMS_TO_TICKS(1000)); }
}

void sd_logging_task(void *pvParameters) {
    log_msg_t msg;
    while (1) {
        if (xQueueReceive(log_queue, &msg, portMAX_DELAY)) {
            ESP_LOGI("SD_LOG", "%s", msg.data);
        }
    }
}

void app_main(void) {
    nvs_init_storage();
    peripherals_init();
    log_queue = xQueueCreate(LOG_QUEUE_SIZE, sizeof(log_msg_t));

    esp_netif_init();
    esp_event_loop_create_default();
    esp_netif_create_default_wifi_ap();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&cfg);
    esp_wifi_set_mode(WIFI_MODE_AP);

    wifi_config_t ap_config = {
        .ap = {
            .ssid = WIFI_SSID,
            .password = WIFI_PASS,
            .ssid_len = strlen(WIFI_SSID),
            .channel = ESPNOW_CHANNEL,
            .max_connection = 4,
            .authmode = WIFI_AUTH_WPA2_PSK
        },
    };
    esp_wifi_set_config(WIFI_IF_AP, &ap_config);
    esp_wifi_start();

    espnow_init_base();

    xTaskCreatePinnedToCore(network_task, "net_task", 4096, NULL, 5, NULL, 1);
    xTaskCreatePinnedToCore(sd_logging_task, "log_task", 4096, NULL, 4, NULL, 1);

    ESP_LOGI(TAG, "VOSTOK NEXUS v5.1 S3 STARTED");
}
