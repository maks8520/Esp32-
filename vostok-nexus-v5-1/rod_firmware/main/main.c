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

static const char *TAG = "VOSTOK_ROD";
uint8_t base_mac[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}; // Broadcast for simplicity in this example

typedef struct {
    uint8_t rod_id;
    float accel_x;
    float accel_y;
    float accel_z;
    uint16_t hall_val;
    uint8_t bite_intensity;
} rod_data_t;

void app_main(void) {
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_ERROR_CHECK(esp_now_init());

    esp_now_peer_info_t peerInfo = {};
    memcpy(peerInfo.peer_addr, base_mac, 6);
    peerInfo.channel = 1;
    peerInfo.encrypt = false;
    ESP_ERROR_CHECK(esp_now_add_peer(&peerInfo));

    rod_data_t my_data;
    my_data.rod_id = 1; // Change for Rod 2

    while(1) {
        // Simulating sensor readings
        my_data.accel_x = (rand() % 2000 - 1000) / 100.0;
        my_data.accel_y = (rand() % 2000 - 1000) / 100.0;
        my_data.accel_z = (rand() % 2000 - 1000) / 100.0;
        my_data.hall_val = rand() % 4096;
        my_data.bite_intensity = (my_data.accel_x > 5.0 || my_data.accel_y > 5.0) ? (rand() % 100) : 0;

        esp_now_send(base_mac, (uint8_t *) &my_data, sizeof(my_data));

        vTaskDelay(pdMS_TO_TICKS(100));
    }
}
