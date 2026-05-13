#ifndef CONFIG_H
#define CONFIG_H

/* WIFI & Network */
#define WIFI_SSID           "VOSTOK_NEXUS_V5"
#define WIFI_PASS           "ultra_premium"
#define WS_SERVER_PORT      80

/* ESP-NOW Config */
#define ESPNOW_CHANNEL      1
#define ROD_COUNT           2

/* GPIO Pins - Base (ESP32-S3) */
#define PIN_I2C_SDA         4
#define PIN_I2C_SCL         5
#define PIN_GPS_RX          16
#define PIN_GPS_TX          17
#define PIN_DS18B20         18
#define PIN_WS2812          48
#define PIN_BUZZER          21
#define PIN_BTN_1           0
#define PIN_BTN_2           1
#define PIN_SD_MISO         13
#define PIN_SD_MOSI         11
#define PIN_SD_CLK          12
#define PIN_SD_CS           10

/* Data Structures */
typedef struct {
    float temp;
    float press;
    float hum;
    float lat;
    float lon;
    float alt;
} base_data_t;

typedef struct {
    uint8_t rod_id;
    float accel_x;
    float accel_y;
    float accel_z;
    uint16_t hall_val;
    uint8_t bite_intensity;
} rod_data_t;

#endif
