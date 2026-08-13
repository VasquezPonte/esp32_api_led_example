idf_component_register(
    SRCS "main.c"
    INCLUDE_DIRS "."
    REQUIRES
        esp_driver_gpio
        https_client
        nvs_flash
        sntp_manager
        wifi_manager
)
