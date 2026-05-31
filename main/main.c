#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_task_wdt.h"
#include "config_nvs.h"
#include "batterie.h"
#include "gpio_handler.h"
#include "state_machine.h"
#include "webserver.h"
#include "telemetry.h"
#include "ota.h"

static const char *TAG = "main";

// Tâche principale machine d'états — priorité haute, 100ms tick
static void state_machine_task(void *arg)
{
    (void)arg;
    esp_task_wdt_add(NULL);  // TWDT : reboot si bloqué > CONFIG_ESP_TASK_WDT_TIMEOUT_S
    while (1) {
        tick_state_machine();
        esp_task_wdt_reset();
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

void app_main(void)
{
    ESP_LOGI(TAG, "=== Irrifrance ESP32 — démarrage ===");

    // Initialisation NVS (obligatoire avant toute lecture config et avant les tests NVS)
    ESP_ERROR_CHECK(config_nvs_init());

#ifdef CONFIG_IRRI_ENABLE_TESTS
    extern void test_calculs_hydraulique_run(void);
    extern void test_calculs_mecanique_run(void);
    extern void test_gpio_run(void);
    extern void test_regulation_run(void);
    extern void test_state_machine_run(void);
    extern void test_config_nvs_run(void);
    test_calculs_hydraulique_run();
    test_calculs_mecanique_run();
    test_gpio_run();
    test_regulation_run();
    test_state_machine_run();
    test_config_nvs_run();
    ESP_LOGI(TAG, "=== Tests terminés ===");
#endif

    // Mesure tension batterie (ADC1 GPIO 36)
    ESP_ERROR_CHECK(batterie_init());

    // GPIO — électrovannes OFF immédiat (fail-safe)
    gpio_handler_init();

    // Machine d'états
    state_machine_init();

    // Réseau + Web UI + OTA (squelettes PR-08)
    ESP_ERROR_CHECK(webserver_init());
    ESP_ERROR_CHECK(ota_init());

    // Telemetry WebSocket 500ms
    ESP_ERROR_CHECK(telemetry_init());

    // Tâche machine d'états (priorité 10 — au-dessus du Wifi)
    xTaskCreate(state_machine_task, "state_machine", 8192, NULL, 10, NULL);

    ESP_LOGI(TAG, "Démarrage terminé — tâches actives");
}
