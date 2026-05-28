#include "test_calculs_hydraulique.h"
#include "../calculs_hydraulique.h"
#include "esp_log.h"
#include <math.h>

static const char *TAG = "test_hydro";

// Tolérance acceptable pour les comparaisons de résultats de table (m³/h)
#define EPSILON_DEBIT  0.15f
// Tolérance pression (bar)
#define EPSILON_PRES   0.05f

static int s_ok = 0;
static int s_total = 0;

static void check_f(const char *nom, float attendu, float obtenu, float eps)
{
    s_total++;
    if (fabsf(attendu - obtenu) <= eps) {
        ESP_LOGI(TAG, "  [OK] %-40s attendu=%.3f obtenu=%.3f", nom, attendu, obtenu);
        s_ok++;
    } else {
        ESP_LOGE(TAG, "  [KO] %-40s attendu=%.3f obtenu=%.3f DELTA=%.3f",
                 nom, attendu, obtenu, fabsf(attendu - obtenu));
    }
}

void test_calculs_hydraulique_run(void)
{
    s_ok = 0;
    s_total = 0;
    ESP_LOGI(TAG, "=== Tests calculs_hydraulique ===");

    // --- calcul_pression_canon ---
    // Installation de référence : 7.0 bar, 330m, terrain plat
    // 7.0 - 2.5 (enrouleur) - 1.1 (330/300 × 1.0) - 0.0 = 3.4 bar
    check_f("p_canon 7.0bar 330m plat",
            3.4f, calcul_pression_canon(7.0f, 330.0f, 0.0f), EPSILON_PRES);

    check_f("p_canon 6.0bar 330m plat",
            2.4f, calcul_pression_canon(6.0f, 330.0f, 0.0f), EPSILON_PRES);

    // Avec dénivelé : 7.0 - 2.5 - 1.1 - (5/10 × 1.0) = 2.9 bar
    check_f("p_canon 7.0bar 330m deniv+5m",
            2.9f, calcul_pression_canon(7.0f, 330.0f, 5.0f), EPSILON_PRES);

    // --- calcul_debit_m3h (valeurs exactes de la table UASA46) ---
    check_f("debit 4.0bar buse 12mm",
            10.8f, calcul_debit_m3h(4.0f, 12), EPSILON_DEBIT);

    check_f("debit 5.0bar buse 18mm",
            30.3f, calcul_debit_m3h(5.0f, 18), EPSILON_DEBIT);

    check_f("debit 6.0bar buse 26mm",
            62.2f, calcul_debit_m3h(6.0f, 26), EPSILON_DEBIT);

    check_f("debit 4.5bar buse 14mm",
            15.6f, calcul_debit_m3h(4.5f, 14), EPSILON_DEBIT);

    // Interpolation pression : 4.25 bar buse 12mm → entre 10.8 et 11.4 → ~11.1
    check_f("debit 4.25bar buse 12mm (interpolation)",
            11.1f, calcul_debit_m3h(4.25f, 12), EPSILON_DEBIT);

    // --- calcul_vitesse_cible_m_min ---
    // débit=24.3 m³/h → 405 L/min, largeur=18m, dose=25mm → 405/(18×25)=0.900 m/min
    float debit_L_min = 24.3f * 1000.0f / 60.0f;
    check_f("vitesse 24.3m3h / 18m / 25mm",
            0.900f, calcul_vitesse_cible_m_min(debit_L_min, 18.0f, 25.0f), 0.01f);

    // Paramètres invalides → 0
    check_f("vitesse largeur=0 (invalide)",
            0.0f, calcul_vitesse_cible_m_min(400.0f, 0.0f, 25.0f), 0.001f);

    ESP_LOGI(TAG, "=== Résultat : %d/%d tests OK ===", s_ok, s_total);
}
