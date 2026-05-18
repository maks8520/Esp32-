#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_now.h"
#include "driver/i2c.h"
#include "driver/gpio.h"

static const char *TAG = "ROD_HUNTER";

// Hardware Configuration
#define I2C_MASTER_SDA_IO           8
#define I2C_MASTER_SCL_IO           9
#define I2C_MASTER_NUM              I2C_NUM_0
#define I2C_MASTER_FREQ_HZ          400000
#define MPU6050_ADDR                0x68
#define PIN_HALL_SENSOR             3

// MPU6050 Registers
#define MPU6050_PWR_MGMT_1          0x6B
#define MPU6050_ACCEL_XOUT_H        0x3B
#define MPU6050_ACCEL_ZOUT_H        0x3F

static uint8_t base_mac[6] = {0x28, 0x84, 0x85, 0x50, 0x79, 0x5D};
static EventGroupHandle_t hopping_event_group;
#define SEND_SUCCESS_BIT BIT0
#define SEND_FAIL_BIT    BIT1

// ESP-NOW Callbacks
static void on_data_sent(const uint8_t *mac_addr, esp_now_send_status_t status) {
    if (status == ESP_NOW_SEND_SUCCESS) {
        xEventGroupSetBits(hopping_event_group, SEND_SUCCESS_BIT);
    } else {
        xEventGroupSetBits(hopping_event_group, SEND_FAIL_BIT);
    }
}

// NVS Logic
uint8_t get_saved_channel() {
    nvs_handle_t handle;
    uint8_t channel = 1;
    if (nvs_open("rod_storage", NVS_READONLY, &handle) == ESP_OK) {
        nvs_get_u8(handle, "wifi_chan", &channel);
        nvs_close(handle);
    }
    return (channel >= 1 && channel <= 13) ? channel : 1;
}

void save_channel_to_nvs(uint8_t channel) {
    nvs_handle_t handle;
    if (nvs_open("rod_storage", NVS_READWRITE, &handle) == ESP_OK) {
        nvs_set_u8(handle, "wifi_chan", channel);
        nvs_commit(handle);
        nvs_close(handle);
    }
}

// Channel Hopping logic
void send_data_with_hopping(uint8_t *data, size_t len) {
    uint8_t channel = get_saved_channel();
    bool success = false;

    for (int attempt = 0; attempt < 14; attempt++) {
        esp_wifi_set_channel(channel, WIFI_SECOND_CHAN_NONE);

        esp_now_peer_info_t peer = {0};
        memcpy(peer.peer_addr, base_mac, 6);
        peer.channel = channel;
        peer.encrypt = false;

        if (esp_now_is_peer_exist(base_mac)) {
            esp_now_mod_peer(&peer);
        } else {
            esp_now_add_peer(&peer);
        }

        xEventGroupClearBits(hopping_event_group, SEND_SUCCESS_BIT | SEND_FAIL_BIT);
        if (esp_now_send(base_mac, data, len) == ESP_OK) {
            EventBits_t bits = xEventGroupWaitBits(hopping_event_group, SEND_SUCCESS_BIT | SEND_FAIL_BIT, pdTRUE, pdFALSE, pdMS_TO_TICKS(50));
            if (bits & SEND_SUCCESS_BIT) {
                save_channel_to_nvs(channel);
                success = true;
                break;
            }
        }
        channel = (channel % 13) + 1;
    }
    if (!success) ESP_LOGD(TAG, "Transmission failed");
}

// Peripheral Initialization
static esp_err_t mpu6050_init() {
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = I2C_MASTER_SDA_IO,
        .scl_io_num = I2C_MASTER_SCL_IO,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = I2C_MASTER_FREQ_HZ,
    };
    i2c_param_config(I2C_MASTER_NUM, &conf);
    i2c_driver_install(I2C_MASTER_NUM, conf.mode, 0, 0, 0);

    uint8_t data[] = {MPU6050_PWR_MGMT_1, 0x00};
    return i2c_master_write_to_device(I2C_MASTER_NUM, MPU6050_ADDR, data, sizeof(data), pdMS_TO_TICKS(100));
}

// 1. & 3. Reading Sensors and Sending Data
void sensor_task(void *pvParameters) {
    char json_buf[128];
    uint8_t raw_data[2];
    int16_t acc_x, acc_z;

    while (1) {
        // Read Accel X
        uint8_t reg_x = MPU6050_ACCEL_XOUT_H;
        i2c_master_write_read_device(I2C_MASTER_NUM, MPU6050_ADDR, &reg_x, 1, raw_data, 2, pdMS_TO_TICKS(50));
        acc_x = (raw_data[0] << 8) | raw_data[1];

        // Read Accel Z
        uint8_t reg_z = MPU6050_ACCEL_ZOUT_H;
        i2c_master_write_read_device(I2C_MASTER_NUM, MPU6050_ADDR, &reg_z, 1, raw_data, 2, pdMS_TO_TICKS(50));
        acc_z = (raw_data[0] << 8) | raw_data[1];

        // 2. Read Hall Sensor
        int hall_state = gpio_get_level(PIN_HALL_SENSOR);

        // 4. Format JSON
        snprintf(json_buf, sizeof(json_buf), "{\"telemetry\": {\"acc_x\": %d, \"acc_z\": %d, \"hall\": %d}}", acc_x, acc_z, hall_state);

        // 5. Send data
        send_data_with_hopping((uint8_t *)json_buf, strlen(json_buf));

        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

void app_main(void) {
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // WiFi Init
    esp_netif_init();
    esp_event_loop_create_default();
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&cfg);
    esp_wifi_set_mode(WIFI_MODE_STA);
    esp_wifi_start();

    // ESP-NOW Init
    hopping_event_group = xEventGroupCreate();
    ESP_ERROR_CHECK(esp_now_init());
    esp_now_register_send_cb(on_data_sent);

    // Peripheral Init
    mpu6050_init();
    gpio_config_t hall_cfg = {
        .pin_bit_mask = (1ULL << PIN_HALL_SENSOR),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
    };
    gpio_config(&hall_cfg);

    xTaskCreate(sensor_task, "sensor_task", 4096, NULL, 10, NULL);
}
