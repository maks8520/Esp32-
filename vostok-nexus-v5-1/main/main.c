#include <stdio.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
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

static const char *TAG = "VOSTOK_BASE";
static httpd_handle_t server = NULL;
static int client_fd = -1;

/* Peripherals Initialization */
static void i2c_master_init() {
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
}

static void uart_gps_init() {
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

/* Logging to SD */
void log_to_sd(const char* json_str) {
    FILE* f = fopen("/sdcard/fishing_logs.jsonl", "a");
    if (f) {
        fprintf(f, "%s\n", json_str);
        fclose(f);
    }
}

/* ESP-NOW Receive Callback */
static void espnow_recv_cb(const esp_now_recv_info_t *recv_info, const uint8_t *data, int len) {
    if (len == sizeof(rod_data_t)) {
        rod_data_t rod;
        memcpy(&rod, data, len);

        char buf[256];
        snprintf(buf, sizeof(buf), "{\"type\":\"rod_data\",\"id\":%d,\"ax\":%.2f,\"ay\":%.2f,\"az\":%.2f,\"bite\":%d}",
                 rod.rod_id, rod.accel_x, rod.accel_y, rod.accel_z, rod.bite_intensity);

        log_to_sd(buf);

        if (client_fd != -1) {
            httpd_ws_frame_t ws_pkt;
            memset(&ws_pkt, 0, sizeof(httpd_ws_frame_t));
            ws_pkt.payload = (uint8_t*)buf;
            ws_pkt.len = strlen(buf);
            ws_pkt.type = HTTPD_WS_TYPE_TEXT;
            httpd_ws_send_frame_async(server, client_fd, &ws_pkt);
        }
    }
}

/* WebSocket Handler */
static esp_err_t ws_handler(httpd_req_t *req) {
    if (req->method == HTTP_GET) {
        client_fd = httpd_req_to_sockfd(req);
        return ESP_OK;
    }
    return ESP_OK;
}

static const httpd_uri_t ws = {
    .uri = "/ws", .method = HTTP_GET, .handler = ws_handler, .is_websocket = true
};

static void start_webserver(void) {
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    if (httpd_start(&server, &config) == ESP_OK) {
        httpd_register_uri_handler(server, &ws);
    }
}

void app_main(void) {
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
    uart_gps_init();
    sd_card_init();
    start_webserver();

    while(1) {
        if (client_fd != -1) {
            char buf[256];
            snprintf(buf, sizeof(buf), "{\"type\":\"base_data\",\"temp\":%.2f,\"press\":%.2f,\"hum\":%.2f}",
                     24.5 + (rand()%10)/10.0, 1013.2 + (rand()%20)/10.0, 45.0 + (rand()%50)/10.0);
            httpd_ws_frame_t ws_pkt;
            memset(&ws_pkt, 0, sizeof(httpd_ws_frame_t));
            ws_pkt.payload = (uint8_t*)buf;
            ws_pkt.len = strlen(buf);
            ws_pkt.type = HTTPD_WS_TYPE_TEXT;
            httpd_ws_send_frame_async(server, client_fd, &ws_pkt);
        }
        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}
