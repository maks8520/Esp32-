#include <string.h>
#include "esp_now_logic.h"
#include "esp_log.h"
#include "esp_http_server.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "cJSON.h"

static const char *TAG = "ESP_NOW_LOGIC";
extern QueueHandle_t log_queue;
extern int client_fd;
extern httpd_handle_t server;

/* Очередь для обработки данных ESP-NOW на выделенном ядре */
QueueHandle_t espnow_data_queue;

typedef struct {
    uint8_t mac[6];
    uint8_t data[sizeof(rod_data_t)];
    int len;
} espnow_event_t;

/* Обратный вызов (ISR context) - просто кидаем в очередь */
static void espnow_recv_cb(const esp_now_recv_info_t *recv_info, const uint8_t *data, int len) {
    espnow_event_t evt;
    memcpy(evt.mac, recv_info->src_addr, 6);
    memcpy(evt.data, data, len > sizeof(evt.data) ? sizeof(evt.data) : len);
    evt.len = len;
    xQueueSend(espnow_data_queue, &evt, 0);
}

/* Задача обработки данных телеметрии (Ядро 0) */
void espnow_processing_task(void *pvParameters) {
    espnow_event_t evt;
    while (1) {
        if (xQueueReceive(espnow_data_queue, &evt, portMAX_DELAY)) {
            if (evt.len == sizeof(rod_data_t)) {
                rod_data_t rod;
                memcpy(&rod, evt.data, evt.len);

                cJSON *root = cJSON_CreateObject();
                cJSON_AddStringToObject(root, "type", "rod_data");
                cJSON_AddNumberToObject(root, "id", rod.rod_id);
                cJSON_AddNumberToObject(root, "bite", rod.bite_intensity);

                char *json_str = cJSON_PrintUnformatted(root);

                /* Логирование в очередь (Core 1) */
                log_msg_t log_msg;
                strncpy(log_msg.data, json_str, sizeof(log_msg.data) - 1);
                xQueueSend(log_queue, &log_msg, 0);

                /* Отправка в WebSocket (Core 1) */
                if (client_fd != -1) {
                    httpd_ws_frame_t ws_pkt = { .payload = (uint8_t*)json_str, .len = strlen(json_str), .type = HTTPD_WS_TYPE_TEXT };
                    httpd_ws_send_frame_async(server, client_fd, &ws_pkt);
                }

                cJSON_Delete(root);
                free(json_str);
            }
        }
    }
}

esp_err_t espnow_init_base(void) {
    espnow_data_queue = xQueueCreate(20, sizeof(espnow_event_t));
    esp_err_t ret = esp_now_init();
    if (ret != ESP_OK) return ret;

    /* Запуск задачи обработки на Ядре 0 (Протокол Bolt) */
    xTaskCreatePinnedToCore(espnow_processing_task, "espnow_proc", 4096, NULL, 10, NULL, 0);

    return esp_now_register_recv_cb(espnow_recv_cb);
}
