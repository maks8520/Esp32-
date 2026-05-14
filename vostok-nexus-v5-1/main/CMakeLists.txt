idf_component_register(SRCS "main.c" "nvs_handler.c" "esp_now_logic.c"
                    INCLUDE_DIRS "."
                    PRIV_REQUIRES nvs_flash esp_wifi esp_event esp_http_server esp_now json
)
