#include "test_state_machine.h"
#include "../state_machine.h"
#include "../gpio_handler.h"
#include "esp_log.h"

static const char *TAG = "test_state";

static int s_ok = 0;
static int s_total = 0;

static void check_bool(const char *nom, bool attendu, bool obtenu)
{
    s_total++;
    if (attendu == obtenu) {
        ESP_LOGI(TAG, "  [OK] %s", nom);
        s_ok++;
    } else {
        ESP_LOGE(TAG, "  [KO] %s : attendu=%d obtenu=%d", nom, (int)attendu, (int)obtenu);
    }
}

static void check_etat(const char *nom, etat_machine_t attendu, etat_machine_t obtenu)
{
    s_total++;
    if (attendu == obtenu) {
        ESP_LOGI(TAG, "  [OK] %s : état %d", nom, (int)obtenu);
        s_ok++;
    } else {
        ESP_LOGE(TAG, "  [KO] %s : attendu=%d obtenu=%d", nom, (int)attendu, (int)obtenu);
    }
}

void test_state_machine_run(void)
{
    s_ok = 0;
    s_total = 0;
    ESP_LOGI(TAG, "=== Tests state_machine (squelette PR-01) ===");

    // Vérification état initial après init
    machine_status_t status;
    state_machine_get_status(&status);

    check_etat("état initial = VEILLE", ETAT_VEILLE, status.etat);
    check_bool("EV1 éteint au démarrage", false, status.ev1);
    check_bool("EV2 éteint au démarrage", false, status.ev2);

    // Commande RESET depuis VEILLE → reste VEILLE
    state_machine_cmd_reset();
    state_machine_get_status(&status);
    check_etat("RESET depuis VEILLE → VEILLE", ETAT_VEILLE, status.etat);

    // Commande STOP depuis VEILLE → ARRET_FINAL
    state_machine_cmd_stop();
    state_machine_get_status(&status);
    check_etat("STOP depuis VEILLE → ARRET_FINAL", ETAT_ARRET_FINAL, status.etat);
    check_bool("EV1 OFF après STOP", false, status.ev1);
    check_bool("EV2 OFF après STOP", false, status.ev2);

    // RESET depuis ARRET_FINAL → VEILLE
    state_machine_cmd_reset();
    state_machine_get_status(&status);
    check_etat("RESET depuis ARRET_FINAL → VEILLE", ETAT_VEILLE, status.etat);

    ESP_LOGI(TAG, "=== Résultat PR-01 : %d/%d tests OK ===", s_ok, s_total);
    ESP_LOGI(TAG, "    Tests complets (transitions, fail-safe, timeout) → PR-05");
}
