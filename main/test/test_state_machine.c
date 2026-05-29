#include "sdkconfig.h"
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

    // injection EN_COURS
    state_machine_test_injecter_etat(ETAT_EN_COURS);
    etat = state_machine_get_etat();
    if (etat == ETAT_EN_COURS) {
        ESP_LOGI(TAG, "PASS injecter EN_COURS");
    } else {
        ESP_LOGE(TAG, "FAIL injecter EN_COURS → état %d", etat);
    }

    // urgence depuis EN_COURS → ARRET_URGENCE
    state_machine_declencher_urgence("test SEC depuis EN_COURS");
    etat = state_machine_get_etat();
    if (etat == ETAT_ARRET_URGENCE) {
        ESP_LOGI(TAG, "PASS urgence depuis EN_COURS → ARRET_URGENCE");
    } else {
        ESP_LOGE(TAG, "FAIL urgence depuis EN_COURS → état %d", etat);
    }

    // reset après urgence depuis EN_COURS → VEILLE
    state_machine_cmd_reset();
    etat = state_machine_get_etat();
    if (etat == ETAT_VEILLE) {
        ESP_LOGI(TAG, "PASS reset apres urgence EN_COURS → VEILLE");
    } else {
        ESP_LOGE(TAG, "FAIL reset apres urgence EN_COURS → état %d", etat);
    }

    // cmd_stop depuis EN_COURS → ARRET_FINAL
    state_machine_test_injecter_etat(ETAT_EN_COURS);
    state_machine_cmd_stop();
    etat = state_machine_get_etat();
    if (etat == ETAT_ARRET_FINAL) {
        ESP_LOGI(TAG, "PASS cmd_stop depuis EN_COURS → ARRET_FINAL");
    } else {
        ESP_LOGE(TAG, "FAIL cmd_stop depuis EN_COURS → état %d", etat);
    }

    // cmd_stop depuis ARRET_URGENCE → noop (reste ARRET_URGENCE)
    state_machine_test_injecter_etat(ETAT_ARRET_URGENCE);
    state_machine_cmd_stop();
    etat = state_machine_get_etat();
    if (etat == ETAT_ARRET_URGENCE) {
        ESP_LOGI(TAG, "PASS cmd_stop noop depuis ARRET_URGENCE");
    } else {
        ESP_LOGE(TAG, "FAIL cmd_stop depuis ARRET_URGENCE → état %d", etat);
    }
    state_machine_cmd_reset();

    // injection OUVERTURE_CANON
    state_machine_test_injecter_etat(ETAT_OUVERTURE_CANON);
    etat = state_machine_get_etat();
    if (etat == ETAT_OUVERTURE_CANON) {
        ESP_LOGI(TAG, "PASS injecter OUVERTURE_CANON");
    } else {
        ESP_LOGE(TAG, "FAIL injecter OUVERTURE_CANON → état %d", etat);
    }

    // injection TEMPO_ARRIVEE
    state_machine_test_injecter_etat(ETAT_TEMPO_ARRIVEE);
    etat = state_machine_get_etat();
    if (etat == ETAT_TEMPO_ARRIVEE) {
        ESP_LOGI(TAG, "PASS injecter TEMPO_ARRIVEE");
    } else {
        ESP_LOGE(TAG, "FAIL injecter TEMPO_ARRIVEE → état %d", etat);
    }

    // urgence depuis PAUSE_PRESSION → ARRET_URGENCE → reset → VEILLE
    state_machine_test_injecter_etat(ETAT_PAUSE_PRESSION);
    state_machine_declencher_urgence("test SEC depuis PAUSE_PRESSION");
    etat = state_machine_get_etat();
    if (etat == ETAT_ARRET_URGENCE) {
        ESP_LOGI(TAG, "PASS urgence depuis PAUSE_PRESSION → ARRET_URGENCE");
    } else {
        ESP_LOGE(TAG, "FAIL urgence depuis PAUSE_PRESSION → état %d", etat);
    }
    state_machine_cmd_reset();
    etat = state_machine_get_etat();
    if (etat == ETAT_VEILLE) {
        ESP_LOGI(TAG, "PASS reset final → VEILLE");
    } else {
        ESP_LOGE(TAG, "FAIL reset final → état %d", etat);
    }

    // T_reload_veille : recharger_config en VEILLE ne plante pas
    state_machine_recharger_config();
    etat = state_machine_get_etat();
    if (etat == ETAT_VEILLE) {
        ESP_LOGI(TAG, "PASS reload config en VEILLE → reste VEILLE");
    } else {
        ESP_LOGE(TAG, "FAIL reload config en VEILLE → état %d", etat);
    }

    // T_reload_hors_veille : recharger_config ignoré hors VEILLE
    state_machine_test_injecter_etat(ETAT_EN_COURS);
    state_machine_recharger_config();
    etat = state_machine_get_etat();
    if (etat == ETAT_EN_COURS) {
        ESP_LOGI(TAG, "PASS reload config hors VEILLE → ignoré (EN_COURS inchangé)");
    } else {
        ESP_LOGE(TAG, "FAIL reload config hors VEILLE → état %d", etat);
    }
    state_machine_cmd_reset();
    state_machine_test_injecter_etat(ETAT_VEILLE);
#endif

    ESP_LOGI(TAG, "=== Fin tests machine d'états ===");
}
