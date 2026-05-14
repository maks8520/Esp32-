#ifndef NVS_HANDLER_H
#define NVS_HANDLER_H

#include "esp_err.h"

#define NVS_NAMESPACE "vostok"

/* Инициализация хранилища */
esp_err_t nvs_init_storage(void);

/* Утилиты сохранения и загрузки */
esp_err_t nvs_save_string(const char* key, const char* value);
esp_err_t nvs_load_string(const char* key, char* out_value, size_t max_len);

#endif
