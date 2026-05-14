#ifndef CONFIG_H
#define CONFIG_H

#include <stdint.h>

/* --- Сетевые настройки --- */
#define WIFI_SSID           "VOSTOK_NEXUS_V5"
#define WIFI_PASS           "ultra_premium"
#define WS_SERVER_PORT      80

/* --- ESP-NOW Конфигурация --- */
#define ESPNOW_CHANNEL      1
#define ROD_COUNT           2

/* --- NVS Keys --- */
#define NVS_NAMESPACE       "vostok"
#define NVS_KEY_WINDY       "windy_key"

/* --- Пин-код (ESP32-S3 DevKit) --- */
/* I2C (BME280, OLED если есть) */
#define PIN_I2C_SDA         4
#define PIN_I2C_SCL         5

/* UART (GPS NEO-7M) */
#define PIN_GPS_TX          1
#define PIN_GPS_RX          2

/* SPI (MicroSD) */
#define PIN_SD_MOSI         11
#define PIN_SD_MISO         13
#define PIN_SD_CLK          12
#define PIN_SD_CS           10

/* OneWire (DS18B20) */
#define PIN_DS18B20         14

/* UI Elements */
#define PIN_WS2812          48
#define PIN_BUZZER          45
#define PIN_BTN_1           0
#define PIN_BTN_2           21

/* --- Задачи и Очереди --- */
#define TASK_STACK_SIZE     8192
#define LOG_QUEUE_SIZE      100

/* --- Структуры данных --- */
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
