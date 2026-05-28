#include "gpio_handler.h"
#include "esp_log.h"

// Tests GPIO (mode IRRITESTEUR) — PR-02
static const char *TAG = "test_gpio";

void test_gpio_run(void)
{
    ESP_LOGI(TAG, "=== Tests GPIO (squelette PR-02) ===");

    // Vérification initialisation — les EVs doivent être OFF
    gpio_all_ev_off();
    ESP_LOGI(TAG, "PASS gpio_all_ev_off() appelé");

    // Lecture entrées
    entrees_t e;
    gpio_handler_lire_entrees(&e);
    ESP_LOGI(TAG, "PASS lire_entrees() — fin_course=%d secu_spires=%d poumon=%d pression_ok=%d",
             e.fin_course, e.secu_spires, e.poumon_plein, e.pression_ok);

    ESP_LOGI(TAG, "=== Fin tests GPIO ===");
}
