#include <stdio.h>
#include <string.h>
#include "nvs_flash.h"
#include "nvs.h"
#include "nvs_handler.h"
#include "esp_log.h"

static const char *TAG = "NVS_HANDLER";

esp_err_t nvs_init_storage(void) {
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    return ret;
}

esp_err_t nvs_save_string(const char* key, const char* value) {
    nvs_handle_t h;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &h);
    if (err != ESP_OK) return err;
    err = nvs_set_str(h, key, value);
    if (err == ESP_OK) nvs_commit(h);
    nvs_close(h);
    return err;
}

esp_err_t nvs_load_string(const char* key, char* out_value, size_t max_len) {
    nvs_handle_t h;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &h);
    if (err != ESP_OK) return err;
    err = nvs_get_str(h, key, out_value, &max_len);
    nvs_close(h);
    return err;
}

esp_err_t nvs_save_i32(const char* key, int32_t value) {
    nvs_handle_t h;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &h);
    if (err != ESP_OK) return err;
    err = nvs_set_i32(h, key, value);
    if (err == ESP_OK) nvs_commit(h);
    nvs_close(h);
    return err;
}

esp_err_t nvs_load_i32(const char* key, int32_t* out_value) {
    nvs_handle_t h;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &h);
    if (err != ESP_OK) return err;
    err = nvs_get_i32(h, key, out_value);
    nvs_close(h);
    return err;
}
