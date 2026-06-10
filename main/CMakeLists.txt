set(SRCS
    "main.c"
    "tpl5010.c"
    "gpio_handler.c"
    "state_machine.c"
    "securites.c"
    "calculs_hydraulique.c"
    "calculs_mecanique.c"
    "regulation.c"
    "batterie.c"
    "ina3221.c"
    "mosfet_surveillance.c"
    "config_nvs.c"
    "webserver.c"
    "telemetry.c"
    "ota.c"
    "machines/machines.c"
    "machines/st1bis_82_330.c"
    "abaques/abaques.c"
    "abaques/sr150c.c"
    "abaques/sr100c.c"
)

if(CONFIG_IRRI_ENABLE_TESTS)
    list(APPEND SRCS
        "test/test_calculs_hydraulique.c"
        "test/test_calculs_mecanique.c"
        "test/test_gpio.c"
        "test/test_regulation.c"
        "test/test_state_machine.c"
        "test/test_config_nvs.c"
        "test/test_mosfet_surveillance.c"
    )
endif()

if(CONFIG_IRRI_TEST_MODE)
    list(APPEND SRCS
        "simulator/simulator.c"
        "simulator/simulator_ws.c"
    )
endif()

# Injecter PROJECT_VER dans le HTML au moment de la configuration CMake
configure_file(${COMPONENT_DIR}/webui/index.html.in ${CMAKE_CURRENT_BINARY_DIR}/webui/index.html @ONLY)

set(EMBED "${CMAKE_CURRENT_BINARY_DIR}/webui/index.html")
if(CONFIG_IRRI_TEST_MODE)
    list(APPEND EMBED "simulator/simulator_ui.html")
endif()

idf_component_register(
    SRCS ${SRCS}
    INCLUDE_DIRS "."
    EMBED_FILES ${EMBED}
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
        app_update
)
