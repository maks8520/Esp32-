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
#include "esp_now.h" // Подключаем для работы моста удочек

// ====================================================================
// НАСТРОЙКА ТВОЕГО ТЕЛЕФОНА (МЕНЯЙ ДАННЫЕ В КАВЫЧКАХ ТУТ)
#define WIFI_SSID "POCO F3"
#define WIFI_PASS "11111111"
// ====================================================================

// Настройки статического IP для Android (192.168.43.xxx). 
// Если у тебя iPhone — замени адрес на 172, 20, 10, 100 и шлюз на 172, 20, 10, 1
#define STATIC_IP_ADDR  192, 168, 43, 100
#define STATIC_GW_ADDR  192, 168, 43, 1
#define STATIC_NETMASK  255, 255, 255, 0

static const char *TAG = "VOSTOK_MAIN";
httpd_handle_t server = NULL;
int client_fd = -1;
QueueHandle_t log_queue;

/**
 * @brief Колбэк приема данных по ESP-NOW от удочек. Транслирует JSON прямо в WebSocket браузера.
 */
void on_base_espnow_recv(const esp_now_recv_info_t *recv_info, const uint8_t *data, int len) {
    if (client_fd >= 0 && server != NULL) {
        httpd_ws_frame_t ws_pkt = {
            .payload = (uint8_t*)data,
            .len = len,
            .type = HTTPD_WS_TYPE_TEXT
        };
        httpd_ws_send_frame_async(server, client_fd, &ws_pkt);
    }
}

/**
 * @brief Обработчик событий Wi-Fi для контроля подключения к смартфону
 */
static void wifi_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data) {
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
        ESP_LOGI(TAG, "Поиск точки доступа смартфона %s...", WIFI_SSID);
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        esp_wifi_connect();
        ESP_LOGW(TAG, "Связь потеряна. Автоматическое переподключение...");
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "=================================================");
        ESP_LOGI(TAG, "БАЗА В СЕТИ! ОТКРОЙ В БРАУЗЕРЕ СМАРТФОНА: " IPSTR, IP2STR(&event->ip_info.ip));
        ESP_LOGI(TAG, "=================================================");
    }
}

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
    config.stack_size = 10240; // Избегаем переполнения стэка при работе с JSON/Вебсокетами

    if (httpd_start(&server, &config) == ESP_OK) {
        httpd_register_uri_handler(server, &ws);
        httpd_register_uri_handler(server, &common_get_uri);
    }
    while(1) { vTaskDelay(pdMS_TO_TICKS(1000)); }
}

/**
 * @brief Задача автоматического парсинга данных GPS NEO-7M (Ядро 0)
 */
void gps_task(void *pvParameters) {
    uint8_t data[256];
    char *line;
    while (1) {
        int len = uart_read_bytes(UART_NUM_1, data, sizeof(data) - 1, pdMS_TO_TICKS(1000));
        if (len > 0) {
            data[len] = '\0';
            line = strstr((char *)data, "$GPGGA");
            if (line) {
                float lat_raw = 0, lon_raw = 0;
                char lat_dir = 0, lon_dir = 0;
                int fix_quality = 0, satellites = 0;
                
                int parsed = sscanf(line, "$GPGGA,%*f,%f,%c,%f,%c,%d,%d", 
                                    &lat_raw, &lat_dir, &lon_raw, &lon_dir, &fix_quality, &satellites);
                
                if (parsed >= 6 && fix_quality > 0) {
                    // Перевод координат NMEA в десятичные градусы (DD.DDDD)
                    float latitude = (int)(lat_raw / 100) + ((lat_raw - ((int)(lat_raw / 100) * 100)) / 60.0);
                    if (lat_dir == 'S') latitude = -latitude;

                    float longitude = (int)(lon_raw / 100) + ((lon_raw - ((int)(lon_raw / 100) * 100)) / 60.0);
                    if (lon_dir == 'W') longitude = -longitude;

                    // Отправка пакета в браузер по WebSocket
                    if (client_fd >= 0) {
                        char json_payload[128];
                        snprintf(json_payload, sizeof(json_payload), 
                                 "{\"gps\": {\"lat\": %.6f, \"lon\": %.6f, \"satellites\": %d}}", 
                                 latitude, longitude, satellites);
                        
                        httpd_ws_frame_t ws_pkt = { .payload = (uint8_t*)json_payload, .len = strlen(json_payload), .type = HTTPD_WS_TYPE_TEXT };
                        httpd_ws_send_frame_async(server, client_fd, &ws_pkt);
                    }
                }
            }
        }
        vTaskDelay(pdMS_TO_TICKS(100));
    }
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
    /* Инициализация NVS, SPIFFS и Периферии */
    nvs_init_storage();
    spiffs_init();
    peripherals_init();
    log_queue = xQueueCreate(LOG_QUEUE_SIZE, sizeof(log_msg_t));

    // Инициализация сетевых интерфейсов под режим Клиента (STA)
    esp_netif_init();
    esp_event_loop_create_default();
    esp_netif_t *sta_netif = esp_netif_create_default_wifi_sta();

    // Фиксация статического IP-адреса Базы в подсети смартфона
    esp_netif_dhcpc_stop(sta_netif);
    esp_netif_ip_info_t ip_info;
    IP4_ADDR(&ip_info.ip, STATIC_IP_ADDR);
    IP4_ADDR(&ip_info.gw, STATIC_GW_ADDR);
    IP4_ADDR(&ip_info.netmask, STATIC_NETMASK);
    esp_netif_set_ip_info(sta_netif, &ip_info);

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&cfg);

    // Регистрация обработчиков событий Wi-Fi
    esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, NULL);
    esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL, NULL);

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = WIFI_SSID,
            .password = WIFI_PASS,
        },
    };
    esp_wifi_set_mode(WIFI_MODE_STA);
    esp_wifi_set_config(WIFI_IF_STA, &wifi_config);
    esp_wifi_start();

    /* Инициализация базового уровня радиопротокола */
    espnow_init_base();
    // Переопределяем встроенный колбэк на наш сквозной WebSocket-мост
    esp_now_register_recv_cb(on_base_espnow_recv);

    /* Распределение задач по ядрам процессора */
    xTaskCreatePinnedToCore(network_stack_task, "net_task", 4096, NULL, 5, NULL, 1);
    xTaskCreatePinnedToCore(sd_log_async_task, "log_task", 4096, NULL, 4, NULL, 1);
    xTaskCreatePinnedToCore(gps_task, "gps_task", 4096, NULL, 3, NULL, 0);

    ESP_LOGI(TAG, "Система VOSTOK NEXUS v5.1 S3 ULTRA PREMIUM запущена.");
}
