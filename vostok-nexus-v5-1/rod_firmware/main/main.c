#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_now.h"
#include "driver/i2c.h"
#include "driver/gpio.h"

/**
 * @brief Пин-маппинг для ESP32-C3 (Удочка).
 * I2C(SDA:8, SCL:9), Hall Sensor(3), WS2812(2).
 */
#define PIN_I2C_SDA     8
#define PIN_I2C_SCL     9
#define PIN_HALL_SENSOR 3
#define PIN_WS2812      2

#define ROD_ID 1
uint8_t base_mac[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

typedef struct {
    uint8_t rod_id;
    float accel_x;
    float accel_y;
    float accel_z;
    uint16_t hall_val;
    uint8_t bite_intensity;
} rod_data_t;

void app_main(void) {
    // Инициализация NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // Настройка GPIO для датчика Холла
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << PIN_HALL_SENSOR),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = 1,
        .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config(&io_conf);

    // WiFi в режиме Station для ESP-NOW
    esp_netif_init();
    esp_event_loop_create_default();
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&cfg);
    esp_wifi_set_mode(WIFI_MODE_STA);
    esp_wifi_start();

    // Инициализация ESP-NOW
    esp_now_init();
    esp_now_peer_info_t peerInfo = {};
    memcpy(peerInfo.peer_addr, base_mac, 6);
    peerInfo.channel = 1;
    peerInfo.encrypt = false;
    esp_now_add_peer(&peerInfo);

    rod_data_t my_data = { .rod_id = ROD_ID };

    while(1) {
        // Симуляция данных датчиков (MPU6050 и Hall)
        my_data.accel_z = 9.8 + (rand() % 30) / 10.0;
        my_data.hall_val = gpio_get_level(PIN_HALL_SENSOR) ? 1024 : 0; // Пример чтения
        my_data.bite_intensity = (my_data.accel_z > 10.5) ? (rand() % 100) : 0;

        esp_now_send(base_mac, (uint8_t *) &my_data, sizeof(my_data));

        vTaskDelay(pdMS_TO_TICKS(100));
    }
}
