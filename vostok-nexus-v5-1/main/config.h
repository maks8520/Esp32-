#ifndef CONFIG_H
#define CONFIG_H

#include <stdint.h>

/* Настройки WiFi и сети */
#define WIFI_SSID           "VOSTOK_NEXUS_V5"
#define WIFI_PASS           "ultra_premium"
#define WS_SERVER_PORT      80

/* Настройки ESP-NOW */
#define ESPNOW_CHANNEL      1
#define ROD_COUNT           2

/* Пространства имен и ключи NVS */
#define NVS_NAMESPACE       "vostok"
#define NVS_KEY_WINDY       "windy_key"

/* Распиновка - Базовая станция (ESP32-S3) */
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
#define PIN_BUTTON_1        6
#define PIN_BUTTON_2        7


/* Конфигурация задач */
#define TASK_STACK_SIZE_CORE0  8192
#define TASK_STACK_SIZE_CORE1  8192
#define LOG_QUEUE_SIZE         100

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
    uint32_t checksum;
} log_msg_t;

#endif
