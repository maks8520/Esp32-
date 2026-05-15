#include "nvs_flash.h"
#include "nvs.h"
#include "nvs_handler.h"
#include "config.h"
#include "esp_log.h"
#include "cJSON.h"

static const char* TAG = "NVS";

void nvs_init_storage(void) {
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
}

esp_err_t nvs_save_string(const char* key, const char* val) {
    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK) return err;
    err = nvs_set_str(handle, key, val);
    nvs_commit(handle);
    nvs_close(handle);
    return err;
}

esp_err_t nvs_get_string(const char* key, char* buf, size_t max_len) {
    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &handle);
    if (err != ESP_OK) return err;
    err = nvs_get_str(handle, key, buf, &max_len);
    nvs_close(handle);
    return err;
}
