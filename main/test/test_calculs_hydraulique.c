#include "test_calculs_hydraulique.h"
#include "../calculs_hydraulique.h"
#include "esp_log.h"
#include <math.h>

static const char *TAG = "test_hydro";

#define EPSILON_V    0.2f   // Tolérance vitesse (m/h)
#define EPSILON_PRES 0.05f  // Tolérance pression (bar)

static int s_ok = 0;
static int s_total = 0;

static void check_f(const char *nom, float attendu, float obtenu, float eps)
{
    s_total++;
    if (fabsf(attendu - obtenu) <= eps) {
        ESP_LOGI(TAG, "  [OK] %-48s attendu=%.3f obtenu=%.3f", nom, attendu, obtenu);
        s_ok++;
    } else {
        ESP_LOGE(TAG, "  [KO] %-48s attendu=%.3f obtenu=%.3f DELTA=%.3f",
                 nom, attendu, obtenu, fabsf(attendu - obtenu));
    }
}

void test_calculs_hydraulique_run(void)
{
    s_ok = 0;
    s_total = 0;
    ESP_LOGI(TAG, "=== Tests calculs_hydraulique ===");

    // --- calcul_pression_canon ---
    // 7.0 - 2.5 (enrouleur) - 1.1 (330/300×1.0) - 0.0 = 3.4 bar
    check_f("p_canon 7.0bar 330m plat",
            3.4f, calcul_pression_canon(7.0f, 330.0f, 0.0f), EPSILON_PRES);

    check_f("p_canon 6.0bar 330m plat",
            2.4f, calcul_pression_canon(6.0f, 330.0f, 0.0f), EPSILON_PRES);

    // 7.0 - 2.5 - 1.1 - (5/10×1.0) = 2.9 bar
    check_f("p_canon 7.0bar 330m deniv+5m",
            2.9f, calcul_pression_canon(7.0f, 330.0f, 5.0f), EPSILON_PRES);

    // --- lookup_vitesse_cible — points exacts de la table ---
    // Entrée 0 : 4.9 bar, 3.5mm, dose 20mm → 15.3 m/h, débit 23.0 m³/h
    float debit = 0.0f;
    check_f("vtbl 4.9bar 3.5mm dose 20mm → 15.3m/h",
            15.3f, lookup_vitesse_cible(4.9f, 3.5f, 20.0f, &debit), EPSILON_V);
    check_f("vtbl 4.9bar 3.5mm débit → 23.0m3/h",
            23.0f, debit, EPSILON_V);

    // Entrée 6 : 7.7 bar, 4.0mm, dose 25mm → 28.1 m/h
    check_f("vtbl 7.7bar 4.0mm dose 25mm → 28.1m/h",
            28.1f, lookup_vitesse_cible(7.7f, 4.0f, 25.0f, NULL), EPSILON_V);

    // Entrée 11 : 9.5 bar, 6.0mm, dose 10mm → 13.6 m/h
    check_f("vtbl 9.5bar 6.0mm dose 10mm → 13.6m/h",
            13.6f, lookup_vitesse_cible(9.5f, 6.0f, 10.0f, NULL), EPSILON_V);

    // --- Interpolation linéaire dose ---
    // Entrée 0 : dose 12.5mm → ½ entre 12.3 (15mm) et 9.6 (10mm)
    // t = (12.5-10)/(15-10) = 0.5 → 9.6 + 0.5×(12.3-9.6) = 9.6 + 1.35 = 10.95 → ~11.0
    check_f("vtbl 4.9bar 3.5mm dose 12.5mm (interpolation)",
            10.95f, lookup_vitesse_cible(4.9f, 3.5f, 12.5f, NULL), EPSILON_V);

    // --- Clampage dose hors bornes ---
    check_f("vtbl dose < 10mm → colonne 10mm",
            9.6f, lookup_vitesse_cible(4.9f, 3.5f, 5.0f, NULL), EPSILON_V);
    check_f("vtbl dose > 30mm → colonne 30mm",
            25.6f, lookup_vitesse_cible(4.9f, 3.5f, 40.0f, NULL), EPSILON_V);

    // --- Nearest-neighbor : point proche de l'entrée 0 ---
    // 5.0 bar / 3.6mm → doit sélectionner entrée 0 (4.9/3.5), dose 20mm → 15.3 m/h
    check_f("vtbl 5.0bar 3.6mm dose 20mm (nearest entry 0)",
            15.3f, lookup_vitesse_cible(5.0f, 3.6f, 20.0f, NULL), EPSILON_V);

    ESP_LOGI(TAG, "=== Résultat : %d/%d tests OK ===", s_ok, s_total);
}
