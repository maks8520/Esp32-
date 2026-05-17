idf_component_register(SRCS "main.c" "nvs_handler.c" "esp_now_logic.c"
                    INCLUDE_DIRS "."
                    PRIV_REQUIRES esp_http_server esp_wifi nvs_flash esp_now cjson fatfs esp_vfs_fat sdmmc
)
