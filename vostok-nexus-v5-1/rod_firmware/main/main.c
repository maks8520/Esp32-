#include <stdio.h>
#include <string.h>
#include <math.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_now.h"
#include "driver/i2c.h"

static const char *TAG = "VOSTOK_NEXUS_ROD";
uint8_t base_mac[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

typedef struct {
    uint8_t rod_id;
    float accel_x;
    float accel_y;
    float accel_z;
    uint16_t hall_val;
    uint8_t bite_intensity;
} rod_data_t;

/* Motion Detection logic to save battery */
bool motion_detected(rod_data_t *old, rod_data_t *new) {
    float diff = sqrt(pow(old->accel_x - new->accel_x, 2) +
                      pow(old->accel_y - new->accel_y, 2) +
                      pow(old->accel_z - new->accel_z, 2));
    return (diff > 0.5); // Threshold for motion
}

void app_main(void) {
    nvs_flash_init();
    esp_netif_init();
    esp_event_loop_create_default();
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&cfg);
    esp_wifi_set_mode(WIFI_MODE_STA);
    esp_wifi_start();

    esp_now_init();
    esp_now_peer_info_t peerInfo = {};
    memcpy(peerInfo.peer_addr, base_mac, 6);
    peerInfo.channel = 1;
    peerInfo.encrypt = false;
    esp_now_add_peer(&peerInfo);

    rod_data_t current_data = { .rod_id = 1 };
    rod_data_t last_sent_data = { 0 };

    while(1) {
        // Mock MPU6050 reading with gravity
        current_data.accel_x = (rand() % 100 - 50) / 100.0;
        current_data.accel_y = (rand() % 100 - 50) / 100.0;
        current_data.accel_z = 9.81 + (rand() % 200 - 100) / 100.0;
        current_data.hall_val = 512 + (rand() % 20 - 10);

        // Calculate bite intensity based on Z-axis vibration
        float z_vibration = fabs(current_data.accel_z - 9.81);
        current_data.bite_intensity = (z_vibration > 1.0) ? (uint8_t)(z_vibration * 10) : 0;

        if (motion_detected(&last_sent_data, &current_data) || current_data.bite_intensity > 0) {
            esp_now_send(base_mac, (uint8_t *) &current_data, sizeof(current_data));
            memcpy(&last_sent_data, &current_data, sizeof(rod_data_t));
            vTaskDelay(pdMS_TO_TICKS(100)); // Active broadcast
        } else {
            vTaskDelay(pdMS_TO_TICKS(1000)); // Low power polling
        }
    }
}
