#include <string.h>
#include "esp_now_logic.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "cJSON.h"

static const char *TAG = "ESP_NOW_LOGIC";
extern QueueHandle_t log_queue;
extern int client_fd;
extern httpd_handle_t server;

static void espnow_recv_cb(const esp_now_recv_info_t *recv_info, const uint8_t *data, int len) {
    if (len == sizeof(rod_data_t)) {
        rod_data_t rod;
        memcpy(&rod, data, len);

        cJSON *root = cJSON_CreateObject();
        cJSON_AddStringToObject(root, "type", "rod_data");
        cJSON_AddNumberToObject(root, "id", rod.rod_id);
        cJSON_AddNumberToObject(root, "ax", rod.accel_x);
        cJSON_AddNumberToObject(root, "ay", rod.accel_y);
        cJSON_AddNumberToObject(root, "az", rod.accel_z);
        cJSON_AddNumberToObject(root, "bite", rod.bite_intensity);

        char *json_str = cJSON_PrintUnformatted(root);

        // Asynchronous logging
        log_msg_t log_msg;
        strncpy(log_msg.data, json_str, sizeof(log_msg.data) - 1);
        xQueueSend(log_queue, &log_msg, 0);

        // Notify UI via WebSocket (Broadcasting if multiple clients)
        if (client_fd != -1) {
            httpd_ws_frame_t ws_pkt = { .payload = (uint8_t*)json_str, .len = strlen(json_str), .type = HTTPD_WS_TYPE_TEXT };
            httpd_ws_send_frame_async(server, client_fd, &ws_pkt);
        }

        cJSON_Delete(root);
        free(json_str);
    }
}

esp_err_t espnow_init_base(void) {
    esp_err_t ret = esp_now_init();
    if (ret != ESP_OK) return ret;
    return esp_now_register_recv_cb(espnow_recv_cb);
}
