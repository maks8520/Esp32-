#ifndef CONFIG_H
#define CONFIG_H

#include <stdint.h>

/* WiFi и Сеть */
#define WIFI_SSID           "VOSTOK_NEXUS_V5"
#define WIFI_PASS           "ultra_premium"
#define WS_SERVER_PORT      80

/* Конфигурация ESP-NOW */
#define ESPNOW_CHANNEL      1
#define ROD_COUNT           2

/* NVS Пространства имен и ключи */
#define NVS_NAMESPACE       "vostok"
#define NVS_KEY_WINDY       "windy_key"
#define WINDY_API_KEY_DEF   "WBCzKeL9AXVHOrcZ4ViyvqpdID2r25LL"

/* Пины - Base (ESP32-S3) */
#define PIN_I2C_SDA         4
#define PIN_I2C_SCL         5
#define PIN_GPS_TX          1
#define PIN_GPS_RX          2
#define PIN_SD_MOSI         11
#define PIN_SD_MISO         13
#define PIN_SD_CLK          12
#define PIN_SD_CS           10
#define PIN_DS18B20         14
#define PIN_WS2812          48
#define PIN_BUZZER          45

/* Настройки задач */
#define TASK_STACK_SIZE     4096
#define LOG_QUEUE_SIZE      50

/* Структуры данных */
typedef struct {
    uint8_t rod_id;
    float accel_x;
    float accel_y;
    float accel_z;
    uint16_t hall_val;
    uint8_t bite_intensity;
} rod_data_t;

typedef struct {
    char data[256];
} log_msg_t;

#endif
