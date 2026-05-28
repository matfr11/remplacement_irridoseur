#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_flash.h"
#include "esp_log.h"

#include "gpio_handler.h"
#include "state_machine.h"
#include "config_nvs.h"
#include "webserver.h"
#include "telemetry.h"

#ifdef CONFIG_IRRI_ENABLE_TESTS
#include "test/test_calculs_hydraulique.h"
#include "test/test_calculs_mecanique.h"
#include "test/test_state_machine.h"
#endif

static const char *TAG = "irrifrance";

// Tâche machine d'états — priorité 5 (haute), tick 100ms
static void state_machine_task(void *pvParam)
{
    (void)pvParam;
    while (1) {
        tick_state_machine();
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

// Tâche diffusion WebSocket — priorité 3 (basse), tick 500ms
static void websocket_task(void *pvParam)
{
    (void)pvParam;
    while (1) {
        ws_broadcast_status();
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

void app_main(void)
{
    ESP_LOGI(TAG, "=== Irrifrance ESP32 — démarrage ===");

    // 1. Initialisation NVS flash (efface si version incompatible)
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_LOGW(TAG, "NVS incompatible — effacement et ré-initialisation");
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // 2. Initialisation configuration (lecture NVS, valeurs par défaut si absent)
    ESP_ERROR_CHECK(config_nvs_init());

    // 3. Chargement de la configuration machine (géométrie + sens contacteurs)
    config_machine_t cfg_machine;
    ESP_ERROR_CHECK(config_nvs_lire_machine(&cfg_machine));

    // 4. Initialisation GPIO + ISR capteur vitesse
    gpio_handler_init();

    // Application du sens des contacteurs depuis NVS (NO/NC configurable)
    gpio_sens_t sens = {
        .contact_no_fin_course      = cfg_machine.contact_no_fin_course,
        .contact_no_securite_spires = cfg_machine.contact_no_securite_spires,
        .contact_no_poumon          = cfg_machine.contact_no_poumon,
        .contact_no_pressostat      = cfg_machine.contact_no_pressostat,
    };
    gpio_handler_set_sens(&sens);

    // 5. Initialisation machine d'états (état VEILLE, EV1=OFF, EV2=OFF)
    state_machine_init();

#ifdef CONFIG_IRRI_ENABLE_TESTS
    ESP_LOGW(TAG, "=== MODE TEST ACTIVÉ (CONFIG_IRRI_ENABLE_TESTS) ===");
    test_calculs_hydraulique_run();
    test_calculs_mecanique_run();
    test_state_machine_run();
    ESP_LOGW(TAG, "=== FIN DES TESTS — démarrage normal ===");
#endif

    // 6. Initialisation WiFi AP + WebServer (implémentés PR-07)
    // wifi_ap_init();
    // ESP_ERROR_CHECK(webserver_start());

    // 7. Démarrage tâches FreeRTOS
    xTaskCreate(state_machine_task, "state_machine",
                4096, NULL, 5, NULL);

    xTaskCreate(websocket_task, "websocket",
                8192, NULL, 3, NULL);

    ESP_LOGI(TAG, "Système opérationnel — tâches FreeRTOS démarrées");
}
