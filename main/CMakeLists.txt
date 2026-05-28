set(SRCS
    "main.c"
    "gpio_handler.c"
    "state_machine.c"
    "calculs_hydraulique.c"
    "calculs_mecanique.c"
    "config_nvs.c"
    "webserver.c"
    "telemetry.c"
)

# Les tests sont compilés uniquement si l'option est activée dans menuconfig
if(CONFIG_IRRI_ENABLE_TESTS)
    list(APPEND SRCS
        "test/test_calculs_hydraulique.c"
        "test/test_calculs_mecanique.c"
        "test/test_state_machine.c"
    )
endif()

idf_component_register(
    SRCS ${SRCS}
    INCLUDE_DIRS "."
    REQUIRES
        driver
        freertos
        nvs_flash
        esp_wifi
        esp_http_server
        esp_netif
        esp_event
        esp_timer
        lwip
        newlib
)
