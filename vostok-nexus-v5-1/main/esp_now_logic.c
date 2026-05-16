#include <string.h>
#include "esp_now_logic.h"
#include "esp_log.h"
#include "esp_http_server.h"

extern int client_fd;
extern httpd_handle_t server;

void on_base_espnow_recv(const esp_now_recv_info_t *recv_info, const uint8_t *data, int len) {
    if (client_fd >= 0 && server != NULL) {
        httpd_ws_frame_t ws_pkt;
        memset(&ws_pkt, 0, sizeof(httpd_ws_frame_t));
        ws_pkt.payload = (uint8_t *)data;
        ws_pkt.len = len;
        ws_pkt.type = HTTPD_WS_TYPE_TEXT;
        httpd_ws_send_frame_async(server, client_fd, &ws_pkt);
    }
}

esp_err_t espnow_init_base(void) {
    return esp_now_init();
}
