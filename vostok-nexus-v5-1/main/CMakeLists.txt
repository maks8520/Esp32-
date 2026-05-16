idf_component_register(SRCS "main.c" "esp_now_logic.c" "nvs_handler.c"
                    INCLUDE_DIRS "."
                    PRIV_REQUIRES esp_http_server esp_wifi nvs_flash esp-now cjson)

# ДОБАВИТЬ ЭТУ СТРОКУ СНИЗУ:
spiffs_create_partition_image(storage ../../web FLASH_IN_PROJECT)
