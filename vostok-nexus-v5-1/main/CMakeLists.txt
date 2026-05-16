idf_component_register(SRCS "main.c" "esp_now_logic.c" "nvs_handler.c"
                    INCLUDE_DIRS "."
                    PRIV_REQUIRES esp_http_server esp_wifi nvs_flash esp-now cjson)

spiffs_create_partition_image(storage "${CMAKE_CURRENT_LIST_DIR}/../web" FLASH_IN_PROJECT)
