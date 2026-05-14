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
#include "driver/i2c.h"
#include "driver/uart.h"
#include "driver/gpio.h"
#include "cJSON.h"
#include "config.h"
#include "nvs_handler.h"
#include "esp_now_logic.h"


static const char *TAG = "VOSTOK_MAIN";
httpd_handle_t server = NULL;
int client_fd = -1;
QueueHandle_t log_queue;

/**
 * @brief Инициализация периферии согласно спецификации
 */
static void peripherals_init() {
    /* I2C: SDA=4, SCL=5 */
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = PIN_I2C_SDA,
        .scl_io_num = PIN_I2C_SCL,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = 100000,
    };
    i2c_param_config(I2C_NUM_0, &conf);
    i2c_driver_install(I2C_NUM_0, conf.mode, 0, 0, 0);

    /* GPS UART: TX=1, RX=2 */
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
        return ESP_OK;
    }
    return ESP_OK;
}

static const httpd_uri_t ws = { .uri = "/ws", .method = HTTP_GET, .handler = ws_handler, .is_websocket = true };

/**
 * @brief Сетевая задача (Ядро 1)
 */
void network_stack_task(void *pvParameters) {
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.core_id = 1;
    if (httpd_start(&server, &config) == ESP_OK) {
        httpd_register_uri_handler(server, &ws);
    }
    while(1) { vTaskDelay(pdMS_TO_TICKS(1000)); }
}

/**
 * @brief Неблокирующее логирование (Ядро 1)
 */
void sd_log_async_task(void *pvParameters) {
    log_msg_t msg;
    while (1) {
        if (xQueueReceive(log_queue, &msg, portMAX_DELAY)) {
            /* Заглушка для записи на SD через SPI (Pins 10,11,12,13) */
            ESP_LOGD(TAG, "Log: %s", msg.data);
        }
    }
}

void app_main(void) {
    /* Инициализация NVS и Периферии */
    nvs_init_storage();
    peripherals_init();
    log_queue = xQueueCreate(LOG_QUEUE_SIZE, sizeof(log_msg_t));

    esp_netif_init();
    esp_event_loop_create_default();
    esp_netif_create_default_wifi_ap();
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&cfg);
    esp_wifi_set_mode(WIFI_MODE_AP);
    esp_wifi_start();

    /* Телеметрия на Ядро 0 */
    espnow_init_base();

    /* Сеть и Логи на Ядро 1 */
    xTaskCreatePinnedToCore(network_stack_task, "net_task", 4096, NULL, 5, NULL, 1);
    xTaskCreatePinnedToCore(sd_log_async_task, "log_task", 4096, NULL, 4, NULL, 1);

    ESP_LOGI(TAG, "Система VOSTOK NEXUS v5.1 S3 ULTRA PREMIUM запущена.");
}
