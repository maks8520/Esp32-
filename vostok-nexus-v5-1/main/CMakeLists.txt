idf_component_register(SRCS "main.c" "esp_now_logic.c" "nvs_handler.c"
                    INCLUDE_DIRS "."
                    PRIV_REQUIRES esp_http_server esp_wifi nvs_flash esp_now)
