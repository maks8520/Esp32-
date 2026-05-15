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
#include "config.h"

static const char *TAG = "VOSTOK_ROD";

uint8_t base_mac[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}; // Broadcast for simplicity in this template

typedef struct {
    uint8_t rod_id;
    float accel_x;
    float accel_y;
    float accel_z;
    uint16_t hall_val;
    uint32_t uptime;
} rod_data_t;

void init_wifi() {
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());
}

void init_esp_now() {
    ESP_ERROR_CHECK(esp_now_init());
    esp_now_peer_info_t peerInfo = {};
    memcpy(peerInfo.peer_addr, base_mac, 6);
    peerInfo.channel = 0;
    peerInfo.encrypt = false;
    ESP_ERROR_CHECK(esp_now_add_peer(&peerInfo));
}

void app_main(void) {
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
      ESP_ERROR_CHECK(nvs_flash_erase());
      ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    init_wifi();
    init_esp_now();

    ESP_LOGI(TAG, "Vostok Nexus v5.1 Rod #%d Initialized", ROD_ID);

    rod_data_t data = { .rod_id = ROD_ID };

    while(1) {
        data.accel_x = 0.1f; // Simulated
        data.accel_y = -0.2f;
        data.accel_z = 9.8f;
        data.hall_val = 512;
        data.uptime = esp_log_timestamp();

        esp_now_send(base_mac, (uint8_t *)&data, sizeof(data));
        vTaskDelay(pdMS_TO_TICKS(100)); // 10Hz transmission
    }
}
