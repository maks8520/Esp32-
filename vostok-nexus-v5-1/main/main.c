#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_http_server.h"
#include "esp_spiffs.h"
#include "driver/sdspi_host.h"
#include "driver/spi_common.h"
#include "sdmmc_cmd.h"
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

// 4. wifi_event_handler
static void wifi_event_handler(void* arg, esp_event_base_t event_base,
                                int32_t event_id, void* event_data) {
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        ESP_LOGI(TAG, "Disconnected from AP, retrying...");
        esp_wifi_connect();
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "Successfully connected. Static IP: " IPSTR, IP2STR(&event->ip_info.ip));
    }
}

static void peripherals_init() {
    // I2C Init
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

    // UART Init
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

    // SPIFFS Init
    esp_vfs_spiffs_conf_t conf = {
      .base_path = "/spiffs",
      .partition_label = NULL,
      .max_files = 5,
      .format_if_mount_failed = true
    };
    esp_vfs_spiffs_register(&conf);

    // SD SPI Init
    sdmmc_host_t host = SDSPI_HOST_DEFAULT();
    spi_bus_config_t bus_cfg = {
        .mosi_io_num = PIN_SD_MOSI,
        .miso_io_num = PIN_SD_MISO,
        .sclk_io_num = PIN_SD_CLK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 4000,
    };
    spi_bus_initialize(host.slot, &bus_cfg, SDSPI_DEFAULT_DMA);
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

// 5. network_stack_task
void network_stack_task(void *pvParameters) {
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.stack_size = 10240; // Requirement: config.stack_size = 10240
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

    // 1. esp_netif_create_default_wifi_sta()
    esp_netif_init();
    esp_event_loop_create_default();
    esp_netif_t *sta_netif = esp_netif_create_default_wifi_sta();

    // 2. Stop DHCP client and set static IP parameters
    esp_netif_dhcpc_stop(sta_netif);
    esp_netif_ip_info_t ip_info;
    esp_netif_str_to_ip4("192.168.43.100", &ip_info.ip);
    esp_netif_str_to_ip4("192.168.43.1", &ip_info.gw);
    esp_netif_str_to_ip4("255.255.255.0", &ip_info.netmask);
    esp_netif_set_ip_info(sta_netif, &ip_info);

    // Register event handlers
    esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, NULL);
    esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL, NULL);

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&cfg);

    // 3. Configure STA mode for SSID "MY_PHONE" and Password "12345678"
    wifi_config_t wifi_config = {
        .sta = {
            .ssid = "MY_PHONE",
            .password = "12345678",
        },
    };
    esp_wifi_set_mode(WIFI_MODE_STA);
    esp_wifi_set_config(WIFI_IF_STA, &wifi_config);
    esp_wifi_start();

    espnow_init_base();

    // network_stack_task initialization
    xTaskCreatePinnedToCore(network_stack_task, "net_stack_task", 10240, NULL, 5, NULL, 1);
    xTaskCreatePinnedToCore(sd_logging_task, "log_task", 4096, NULL, 4, NULL, 1);

    ESP_LOGI(TAG, "VOSTOK NEXUS v5.1 S3 STARTED (STA MODE, STATIC IP)");
}
