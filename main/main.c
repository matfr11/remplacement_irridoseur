#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_task_wdt.h"
#include "config_nvs.h"
#include "batterie.h"
#include "ina3221.h"
#include "mosfet_surveillance.h"
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

#ifdef CONFIG_IRRI_WOKWI_MODE
// Simule une perte de pression à t=12s puis restaure à t=15s
static void wokwi_pression_task(void *arg)
{
    vTaskDelay(pdMS_TO_TICKS(12000));
    state_machine_wokwi_set_pression(false);
    ESP_LOGI(TAG, "SIM:P0");
    vTaskDelay(pdMS_TO_TICKS(3000));
    state_machine_wokwi_set_pression(true);
    ESP_LOGI(TAG, "SIM:P1");
    vTaskDelete(NULL);
}
#endif

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
    extern void test_mosfet_surveillance_run(void);
    test_calculs_hydraulique_run();
    test_calculs_mecanique_run();
    test_gpio_run();
    test_regulation_run();
    test_state_machine_run();
    test_config_nvs_run();
    test_mosfet_surveillance_run();
    ESP_LOGI(TAG, "=== Tests terminés ===");
#endif

    // GPIO — électrovannes OFF immédiat (fail-safe), init OUT3/OUT4 et relais
    gpio_handler_init();

#ifndef CONFIG_IRRI_TEST_MODE
    // INA3221 I2C — CH1=EV_CANON, CH2=EV_POUMON, CH3=Batterie
    if (ina3221_init() != ESP_OK)
        ESP_LOGW(TAG, "INA3221 non disponible — surveillance MOSFET et batterie désactivées");

    // Surveillance MOSFETs — GPIO relais et secours
    mosfet_surveillance_init();
#endif

    // Batterie (utilise INA3221 CH3)
    ESP_ERROR_CHECK(batterie_init());

#ifndef CONFIG_IRRI_TEST_MODE
    // Test MOSFETs au démarrage (~300ms)
    if (!mosfet_test_demarrage())
        ESP_LOGE(TAG, "Défaillance MOSFETs critique au démarrage");
#endif

    // Machine d'états
#ifdef CONFIG_IRRI_WOKWI_MODE
    config_programme_t prog_test = {
        .nom = "Test Wokwi",
        .dose_mm = 25.0f, .largeur_m = 10.0f,
        .buse_mm = 5, .pression_bar = 3.0f,
        .tempo_depart_on = false, .tempo_arrivee_on = false,
    };
    config_nvs_sauver_programme(0, &prog_test);
    // mode_deg_poumon : auto-transition REMPLISSAGE_POUMON→EN_COURS après 5 s sans capteur
    config_machine_t cfg_wokwi = CFG_MACHINE_DEFAUT;
    cfg_wokwi.mode_deg_poumon = true;
    cfg_wokwi.t_rempl_fixe_s  = 5.0f;
    config_nvs_sauver_machine(&cfg_wokwi);
#endif
    state_machine_init();
#ifdef CONFIG_IRRI_WOKWI_MODE
    xTaskCreate(wokwi_pression_task, "wokwi_pression", 2048, NULL, 5, NULL);
#endif

#ifndef CONFIG_IRRI_WOKWI_MODE
    // Réseau + Web UI + OTA
    ESP_ERROR_CHECK(webserver_init());
    ESP_ERROR_CHECK(ota_init());

    // Telemetry WebSocket 500ms
    ESP_ERROR_CHECK(telemetry_init());
#endif

    // Tâche machine d'états (priorité 10 — au-dessus du Wifi)
    xTaskCreate(state_machine_task, "state_machine", 8192, NULL, 10, NULL);

    ESP_LOGI(TAG, "Démarrage terminé — tâches actives");
}
