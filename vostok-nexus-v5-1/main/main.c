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
#include "esp_spiffs.h"

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

    /* Настройка кнопок на вход с подтяжкой к питанию */
    gpio_config_t btn_conf = {
        .pin_bit_mask = (1ULL << PIN_BUTTON_1) | (1ULL << PIN_BUTTON_2),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config(&btn_conf);

}

/**
 * @brief Инициализация файловой системы SPIFFS
 */
static void spiffs_init() {
    esp_vfs_spiffs_conf_t conf = {
        .base_path = "/spiffs",
        .partition_label = "storage",
        .max_files = 5,
        .format_if_mount_failed = true
    };
    esp_vfs_spiffs_register(&conf);
}

/**
 * @brief Универсальный обработчик для отдачи статических файлов веб-интерфейса
 */
static esp_err_t common_get_handler(httpd_req_t *req) {
    char filepath[1100]; 
    const char *uri = req->uri;

    if (strcmp(uri, "/") == 0) {
        uri = "/index.html";
    }

    snprintf(filepath, sizeof(filepath), "/spiffs%s", uri);

    FILE *f = fopen(filepath, "r");
    if (f == NULL) {
        httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "Файл не найден");
        return ESP_FAIL;
    }

    if (strstr(uri, ".html")) httpd_resp_set_type(req, "text/html");
    else if (strstr(uri, ".css")) httpd_resp_set_type(req, "text/css");
    else if (strstr(uri, ".js")) httpd_resp_set_type(req, "application/javascript");
    else if (strstr(uri, ".json")) httpd_resp_set_type(req, "application/json");

    char buffer[1024];
    size_t read_bytes;
    while ((read_bytes = fread(buffer, 1, sizeof(buffer), f)) > 0) {
        httpd_resp_send_chunk(req, buffer, read_bytes);
    }
    fclose(f);
    httpd_resp_send_chunk(req, NULL, 0);
    return ESP_OK;
}

/* Структура URI для статики */
static const httpd_uri_t common_get_uri = {
    .uri      = "/*",
    .method   = HTTP_GET,
    .handler  = common_get_handler
};

/**
 * @brief Обработчик WebSocket соединений
 */
static esp_err_t ws_handler(httpd_req_t *req) {
    if (req->method == HTTP_GET) {
        client_fd = httpd_req_to_sockfd(req);
        return ESP_OK;
    }
    return ESP_OK;
}

/* Структура URI для WebSocket */
static const httpd_uri_t ws = { 
    .uri = "/ws", 
    .method = HTTP_GET, 
    .handler = ws_handler, 
    .is_websocket = true 
};

/**
 * @brief Сетевая задача (Ядро 1)
 */
void network_stack_task(void *pvParameters) {
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.core_id = 1;
    config.uri_match_fn = httpd_uri_match_wildcard;
    config.stack_size = 10240; // <--- НАШЕ ИСПРАВЛЕНИЕ: ВЫДЕЛИЛИ 10 КБ СТЭКА, ТЕПЕРЬ ПЕРЕПОЛНЕНИЯ НЕ БУДЕТ!

    if (httpd_start(&server, &config) == ESP_OK) {
        httpd_register_uri_handler(server, &ws);
        httpd_register_uri_handler(server, &common_get_uri);
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


/**
 * @brief Задача для чтения и парсинга NMEA логов с GPS модуля
 */
void gps_task(void *pvParameters) {
    uint8_t data[256];
    char line[256];
    int line_len = 0;

    while (1) {
        int rx_bytes = uart_read_bytes(UART_NUM_1, data, sizeof(data) - 1, pdMS_TO_TICKS(100));
        if (rx_bytes > 0) {
            for (int i = 0; i < rx_bytes; i++) {
                if (data[i] == '\n' || data[i] == '\r') {
                    if (line_len > 0) {
                        line[line_len] = '\0';

                        /* Поиск строки $GPGGA */
                        if (strncmp(line, "$GPGGA", 6) == 0) {
                            float raw_lat, raw_lon;
                            char lat_dir, lon_dir;
                            int fix_quality, satellites;

                            /* Парсинг $GPGGA: $GPGGA,time,lat,N/S,lon,E/W,fix,satellites,... */
                            if (sscanf(line, "$GPGGA,%*f,%f,%c,%f,%c,%d,%d", &raw_lat, &lat_dir, &raw_lon, &lon_dir, &fix_quality, &satellites) == 6) {
                                if (fix_quality > 0) {
                                    /* Перенос координат из DDMM.MMMM в Decimal Degrees */
                                    int lat_deg = (int)(raw_lat / 100);
                                    float lat_min = raw_lat - (lat_deg * 100);
                                    float lat_dd = lat_deg + (lat_min / 60.0f);
                                    if (lat_dir == 'S') lat_dd = -lat_dd;

                                    int lon_deg = (int)(raw_lon / 100);
                                    float lon_min = raw_lon - (lon_deg * 100);
                                    float lon_dd = lon_deg + (lon_min / 60.0f);
                                    if (lon_dir == 'W') lon_dd = -lon_dd;

                                    /* Формирование JSON строки */
                                    char json_str[128];
                                    snprintf(json_str, sizeof(json_str), "{\"gps\": {\"lat\": %.6f, \"lon\": %.6f, \"satellites\": %d}}", lat_dd, lon_dd, satellites);

                                    /* Асинхронная отправка JSON в активный WebSocket-клиент */
                                    if (client_fd >= 0) {
                                        httpd_ws_frame_t ws_pkt;
                                        memset(&ws_pkt, 0, sizeof(httpd_ws_frame_t));
                                        ws_pkt.payload = (uint8_t*)json_str;
                                        ws_pkt.len = strlen(json_str);
                                        ws_pkt.type = HTTPD_WS_TYPE_TEXT;

                                        httpd_ws_send_frame_async(server, client_fd, &ws_pkt);
                                    }
                                }
                            }
                        }
                        line_len = 0;
                    }
                } else {
                    if (line_len < sizeof(line) - 1) {
                        line[line_len++] = (char)data[i];
                    }
                }
            }
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}


/**
 * @brief Задача опроса кнопок с программным антидребезгом
 */
void base_buttons_task(void *pvParameters) {
    int btn1_state = 1;
    int btn2_state = 1;
    int last_btn1_state = 1;
    int last_btn2_state = 1;

    while (1) {
        int current_btn1 = gpio_get_level(PIN_BUTTON_1);
        int current_btn2 = gpio_get_level(PIN_BUTTON_2);

        /* Проверка кнопки 1 */
        if (current_btn1 != last_btn1_state) {
            vTaskDelay(pdMS_TO_TICKS(50)); /* Антидребезг */
            current_btn1 = gpio_get_level(PIN_BUTTON_1);
            if (current_btn1 != last_btn1_state) {
                if (current_btn1 == 0) { /* Нажатие (переход с 1 на 0) */
                    log_msg_t msg;
                    snprintf(msg.data, sizeof(msg.data), "BUTTON 1 PRESSED");
                    xQueueSend(log_queue, &msg, 0);
                    ESP_LOGI(TAG, "BUTTON 1 PRESSED");
                }
                last_btn1_state = current_btn1;
            }
        }

        /* Проверка кнопки 2 */
        if (current_btn2 != last_btn2_state) {
            vTaskDelay(pdMS_TO_TICKS(50)); /* Антидребезг */
            current_btn2 = gpio_get_level(PIN_BUTTON_2);
            if (current_btn2 != last_btn2_state) {
                if (current_btn2 == 0) { /* Нажатие (переход с 1 на 0) */
                    log_msg_t msg;
                    snprintf(msg.data, sizeof(msg.data), "BUTTON 2 PRESSED");
                    xQueueSend(log_queue, &msg, 0);
                    ESP_LOGI(TAG, "BUTTON 2 PRESSED");
                }
                last_btn2_state = current_btn2;
            }
        }

        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

void app_main(void) {
    /* Инициализация NVS, SPIFFS и Периферии */
    nvs_init_storage();
    spiffs_init();
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

    /* Задача GPS на Ядро 0 */
    xTaskCreatePinnedToCore(gps_task, "gps_task", 4096, NULL, 3, NULL, 0);



    /* Задача кнопок на Ядро 1 */
    xTaskCreatePinnedToCore(base_buttons_task, "buttons_task", 4096, NULL, 3, NULL, 1);

    ESP_LOGI(TAG, "Система VOSTOK NEXUS v5.1 S3 ULTRA PREMIUM запущена.");
}
