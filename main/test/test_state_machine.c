#include "state_machine.h"
#include "esp_log.h"

// Tests simulation machine d'états — PR-05
static const char *TAG = "test_sm";

void test_state_machine_run(void)
{
    ESP_LOGI(TAG, "=== Tests machine d'états (squelette PR-05) ===");

    // Vérification état initial
    etat_machine_t etat = state_machine_get_etat();
    if (etat == ETAT_VEILLE) {
        ESP_LOGI(TAG, "PASS état initial = VEILLE");
    } else {
        ESP_LOGE(TAG, "FAIL état initial = %d (attendu VEILLE)", etat);
    }

    // Injection urgence
#ifdef CONFIG_IRRI_ENABLE_TESTS
    state_machine_declencher_urgence("Test urgence simulée");
    etat = state_machine_get_etat();
    if (etat == ETAT_ARRET_URGENCE) {
        ESP_LOGI(TAG, "PASS urgence → ARRET_URGENCE");
    } else {
        ESP_LOGE(TAG, "FAIL urgence → état %d", etat);
    }

    state_machine_cmd_reset();
    etat = state_machine_get_etat();
    if (etat == ETAT_VEILLE) {
        ESP_LOGI(TAG, "PASS cmd_reset → VEILLE");
    } else {
        ESP_LOGE(TAG, "FAIL cmd_reset → état %d", etat);
    }
#endif

    ESP_LOGI(TAG, "=== Fin tests machine d'états ===");
}
