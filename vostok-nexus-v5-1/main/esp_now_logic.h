#ifndef ESP_NOW_LOGIC_H
#define ESP_NOW_LOGIC_H

#include "esp_now.h"
#include "esp_err.h"
#include "config.h"

esp_err_t espnow_init_base(void);
void on_base_espnow_recv(const esp_now_recv_info_t *recv_info, const uint8_t *data, int len);

#endif
