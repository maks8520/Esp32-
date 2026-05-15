#ifndef CONFIG_H
#define CONFIG_H

// Hardware Mapping - Base S3
#define PIN_I2C_SDA      8
#define PIN_I2C_SCL      9
#define PIN_UART_GPS_TX  43
#define PIN_UART_GPS_RX  44
#define PIN_SPI_MOSI     11
#define PIN_SPI_MISO     13
#define PIN_SPI_CLK      12
#define PIN_SPI_CS       10
#define PIN_ONEWIRE_DS   14
#define PIN_WS2812       48
#define PIN_BUZZER       21
#define PIN_BTN_1        0
#define PIN_BTN_2        1

// Network
#define WIFI_SSID "VOSTOK_NEXUS_V5"
#define WIFI_PASS "ultrapremium"
#define WS_PORT   80

// Telemetry
#define MAX_RODS 2

#endif
