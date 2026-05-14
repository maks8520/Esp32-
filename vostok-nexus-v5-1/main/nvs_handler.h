#ifndef NVS_HANDLER_H
#define NVS_HANDLER_H

#include "esp_err.h"

#define NVS_NAMESPACE "vostok"

esp_err_t nvs_init_storage(void);
esp_err_t nvs_save_string(const char* key, const char* value);
esp_err_t nvs_load_string(const char* key, char* out_value, size_t max_len);
esp_err_t nvs_save_i32(const char* key, int32_t value);
esp_err_t nvs_load_i32(const char* key, int32_t* out_value);

#endif
