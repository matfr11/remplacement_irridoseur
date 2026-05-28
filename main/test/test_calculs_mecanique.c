#include "test_calculs_mecanique.h"
#include "../calculs_mecanique.h"
#include "esp_log.h"
#include <math.h>

static const char *TAG = "test_meca";
#define EPSILON  0.001f

static int s_ok = 0;
static int s_total = 0;

static void check_f(const char *nom, float attendu, float obtenu)
{
    s_total++;
    if (fabsf(attendu - obtenu) <= EPSILON) {
        ESP_LOGI(TAG, "  [OK] %-40s attendu=%.4f obtenu=%.4f", nom, attendu, obtenu);
        s_ok++;
    } else {
        ESP_LOGE(TAG, "  [KO] %-40s attendu=%.4f obtenu=%.4f DELTA=%.4f",
                 nom, attendu, obtenu, fabsf(attendu - obtenu));
    }
}

static void check_i(const char *nom, int attendu, int obtenu)
{
    s_total++;
    if (attendu == obtenu) {
        ESP_LOGI(TAG, "  [OK] %-40s attendu=%d obtenu=%d", nom, attendu, obtenu);
        s_ok++;
    } else {
        ESP_LOGE(TAG, "  [KO] %-40s attendu=%d obtenu=%d", nom, attendu, obtenu);
    }
}

void test_calculs_mecanique_run(void)
{
    s_ok = 0;
    s_total = 0;
    ESP_LOGI(TAG, "=== Tests calculs_mecanique ===");

    // Paramètres de référence (SPECS.md section 5.1)
    // R_tambour_vide = 0.25m, d_tuyau_ext = 0.09m
    const float r0 = 0.25f, d = 0.09f;

    // --- calcul_rayon_etage ---
    // R_1 = 0.25 + (1-0.5)×0.09 = 0.25 + 0.045 = 0.295 m
    check_f("rayon etage 1", 0.295f, calcul_rayon_etage(1, r0, d));
    // R_2 = 0.25 + 1.5×0.09 = 0.385 m
    check_f("rayon etage 2", 0.385f, calcul_rayon_etage(2, r0, d));
    // R_3 = 0.25 + 2.5×0.09 = 0.475 m
    check_f("rayon etage 3", 0.475f, calcul_rayon_etage(3, r0, d));
    // R_4 = 0.25 + 3.5×0.09 = 0.565 m
    check_f("rayon etage 4", 0.565f, calcul_rayon_etage(4, r0, d));

    // --- calcul_dist_pulse_m ---
    // dist = 2π × 0.295 / 10 = 0.18535 m
    check_f("dist_pulse etage 1 (R=0.295m)", 0.18535f, calcul_dist_pulse_m(0.295f));
    check_f("dist_pulse etage 2 (R=0.385m)", 0.24190f, calcul_dist_pulse_m(0.385f));

    // --- calcul_dist_poumon_m ---
    // alpha=36° → 1/10 tour → dist = 0.1 × 2π × 0.295 = 0.18535 m
    check_f("dist_poumon etage1 alpha=36°", 0.18535f,
            calcul_dist_poumon_m(0.295f, 36.0f));
    // alpha=18° → 1/20 tour
    check_f("dist_poumon etage1 alpha=18°", 0.09268f,
            calcul_dist_poumon_m(0.295f, 18.0f));

    // --- calcul_freq_poumon ---
    // vitesse=0.9 m/min, dist=0.18535m → freq = 0.9/0.18535 = 4.856 cycles/min
    check_f("freq_poumon 0.9m/min dist=0.18535m",
            4.856f, calcul_freq_poumon(0.9f, 0.18535f));

    // dist invalide → 0
    check_f("freq_poumon dist=0 (invalide)",
            0.0f, calcul_freq_poumon(0.9f, 0.0f));

    // --- calcul_etage_courant ---
    // 0 impulsions → étage 1
    check_i("etage_courant 0 impulsions", 1,
            calcul_etage_courant(0, r0, d, 330.0f, 4));

    // Paramètres non mesurés (r0=0) → retour 1 par défaut
    check_i("etage_courant params invalides", 1,
            calcul_etage_courant(100, 0.0f, 0.0f, 330.0f, 4));

    ESP_LOGI(TAG, "=== Résultat : %d/%d tests OK ===", s_ok, s_total);
}
