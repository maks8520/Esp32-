idf_component_register(SRCS "main.c"
                    INCLUDE_DIRS "."
                    REQUIRES esp_now nvs_flash esp_http_server fatfs esp_vfs_fat)
