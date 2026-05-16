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

static const char *TAG = "ROD_HUNTER";

// 1. Фиксированный MAC-адрес целевой Базы
static uint8_t base_mac[6] = {0x28, 0x84, 0x85, 0x50, 0x79, 0x5D};

static EventGroupHandle_t hopping_event_group;
#define SEND_SUCCESS_BIT BIT0
#define SEND_FAIL_BIT    BIT1

// Колбэк отправки для отслеживания ACK
static void on_data_sent(const uint8_t *mac_addr, esp_now_send_status_t status) {
    if (status == ESP_NOW_SEND_SUCCESS) {
        xEventGroupSetBits(hopping_event_group, SEND_SUCCESS_BIT);
    } else {
        xEventGroupSetBits(hopping_event_group, SEND_FAIL_BIT);
    }
}

// 2. Функции работы с NVS
uint8_t get_saved_channel() {
    nvs_handle_t handle;
    uint8_t channel = 1;
    esp_err_t err = nvs_open("rod_storage", NVS_READONLY, &handle);
    if (err == ESP_OK) {
        nvs_get_u8(handle, "wifi_chan", &channel);
        nvs_close(handle);
    }
    return (channel >= 1 && channel <= 13) ? channel : 1;
}

void save_channel_to_nvs(uint8_t channel) {
    nvs_handle_t handle;
    esp_err_t err = nvs_open("rod_storage", NVS_READWRITE, &handle);
    if (err == ESP_OK) {
        nvs_set_u8(handle, "wifi_chan", channel);
        nvs_commit(handle);
        nvs_close(handle);
    }
}

// 3. Алгоритм Channel Hopping (Охотник)
void send_data_with_hopping(uint8_t *data, size_t len) {
    uint8_t channel = get_saved_channel();
    bool success = false;

    for (int attempt = 0; attempt < 14; attempt++) {
        ESP_LOGD(TAG, "Trying channel %d (attempt %d)", channel, attempt + 1);

        // Настройка радио
        esp_wifi_set_channel(channel, WIFI_SECOND_CHAN_NONE);

        // Конфигурация пира
        esp_now_peer_info_t peer = {0};
        memcpy(peer.peer_addr, base_mac, 6);
        peer.channel = channel;
        peer.encrypt = false;

        if (esp_now_is_peer_exist(base_mac)) {
            esp_now_mod_peer(&peer);
        } else {
            esp_now_add_peer(&peer);
        }

        // Сброс флагов и отправка
        xEventGroupClearBits(hopping_event_group, SEND_SUCCESS_BIT | SEND_FAIL_BIT);
        esp_err_t err = esp_now_send(base_mac, data, len);

        if (err == ESP_OK) {
            // Ожидание ACK до 50 мс
            EventBits_t bits = xEventGroupWaitBits(hopping_event_group,
                                                  SEND_SUCCESS_BIT | SEND_FAIL_BIT,
                                                  pdTRUE, pdFALSE, pdMS_TO_TICKS(50));

            if (bits & SEND_SUCCESS_BIT) {
                ESP_LOGI(TAG, "Success on channel %d", channel);
                save_channel_to_nvs(channel);
                success = true;
                break;
            }
        }

        // Переход на следующий канал
        channel++;
        if (channel > 13) channel = 1;
    }

    if (!success) {
        ESP_LOGW(TAG, "Failed to find Base Station after 14 attempts");
    }
}

void app_main(void) {
    // Инициализация NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // Инициализация Wi-Fi
    esp_netif_init();
    esp_event_loop_create_default();
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&cfg);
    esp_wifi_set_mode(WIFI_MODE_STA);
    esp_wifi_start();

    // Инициализация ESP-NOW
    hopping_event_group = xEventGroupCreate();
    ESP_ERROR_CHECK(esp_now_init());
    esp_now_register_send_cb(on_data_sent);

    // Пример рабочего цикла
    uint8_t payload[] = "{\"id\":1,\"bite\":10,\"accel\":12.5}";

    while(1) {
        send_data_with_hopping(payload, sizeof(payload));
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}
