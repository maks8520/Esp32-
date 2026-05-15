#ifndef NVS_HANDLER_H
#define NVS_HANDLER_H
#include "esp_err.h"
void nvs_init_storage(void);
esp_err_t nvs_save_string(const char* key, const char* val);
esp_err_t nvs_get_string(const char* key, char* buf, size_t max_len);
#endif
