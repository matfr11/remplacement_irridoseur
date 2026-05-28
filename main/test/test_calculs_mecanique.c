#include "test_calculs_mecanique.h"
#include "../calculs_mecanique.h"
#include "esp_log.h"
#include <math.h>

static const char *TAG = "test_meca";

static int s_ok = 0;
static int s_total = 0;

static void check_f(const char *nom, float attendu, float obtenu, float eps)
{
    s_total++;
    if (fabsf(attendu - obtenu) <= eps) {
        ESP_LOGI(TAG, "  [OK] %-52s attendu=%.4f obtenu=%.4f", nom, attendu, obtenu);
        s_ok++;
    } else {
        ESP_LOGE(TAG, "  [KO] %-52s attendu=%.4f obtenu=%.4f DELTA=%.4f",
                 nom, attendu, obtenu, fabsf(attendu - obtenu));
    }
}

static void check_i(const char *nom, int attendu, int obtenu)
{
    s_total++;
    if (attendu == obtenu) {
        ESP_LOGI(TAG, "  [OK] %-52s attendu=%d obtenu=%d", nom, attendu, obtenu);
        s_ok++;
    } else {
        ESP_LOGE(TAG, "  [KO] %-52s attendu=%d obtenu=%d", nom, attendu, obtenu);
    }
}

void test_calculs_mecanique_run(void)
{
    s_ok = 0;
    s_total = 0;
    ESP_LOGI(TAG, "=== Tests calculs_mecanique ===");

    // Paramètres de référence (fiche technique Structure 1 bis)
    const float r0 = 0.25f, d = 0.09f;

    // --- calcul_rayon_etage ---
    // R_1 = 0.25 + 0.5×0.09 = 0.295 m
    check_f("rayon etage 1",  0.295f, calcul_rayon_etage(1, r0, d), 0.001f);
    check_f("rayon etage 2",  0.385f, calcul_rayon_etage(2, r0, d), 0.001f);
    check_f("rayon etage 3",  0.475f, calcul_rayon_etage(3, r0, d), 0.001f);
    check_f("rayon etage 4",  0.565f, calcul_rayon_etage(4, r0, d), 0.001f);

    // --- calcul_dist_pulse_m ---
    // dist = 2π × 0.295 / 10 = 0.18535 m
    check_f("dist_pulse etage 1 (R=0.295m)", 0.18535f, calcul_dist_pulse_m(0.295f), 0.001f);
    check_f("dist_pulse etage 2 (R=0.385m)", 0.24190f, calcul_dist_pulse_m(0.385f), 0.001f);

    // --- calcul_etage_courant ---
    check_i("etage_courant 0 impulsions",
            1, calcul_etage_courant(0, r0, d, 330.0f, 4));
    check_i("etage_courant params invalides",
            1, calcul_etage_courant(100, 0.0f, 0.0f, 330.0f, 4));

    // --- update_dist_par_cycle / get_dist_par_cycle ---
    ESP_LOGI(TAG, "-- dist_par_cycle (moyenne glissante) --");

    update_dist_par_cycle(0.20f);
    check_f("dist_cycle 1 mesure",          0.200f, get_dist_par_cycle_m(), 0.001f);

    update_dist_par_cycle(0.22f);
    check_f("dist_cycle 2 mesures (0.210m)", 0.210f, get_dist_par_cycle_m(), 0.001f);

    update_dist_par_cycle(0.18f);
    update_dist_par_cycle(0.18f);
    update_dist_par_cycle(0.18f);
    // buf = [0.20, 0.22, 0.18, 0.18, 0.18] → moy = 0.192
    check_f("dist_cycle 5 mesures (0.192m)", 0.192f, get_dist_par_cycle_m(), 0.002f);

    // 6e mesure : écrase la plus ancienne → moy change
    update_dist_par_cycle(0.24f);
    float moy6 = get_dist_par_cycle_m();
    check_f("dist_cycle 6e mesure glissante (≠0.192)", 1.0f,
            (fabsf(moy6 - 0.192f) > 0.001f) ? 1.0f : 0.0f, 0.001f);

    // --- calcul_t_attente_s ---
    ESP_LOGI(TAG, "-- calcul_t_attente_s --");

    // v = 9.6 m/h = 0.002667 m/s, dist=0.20m → T_cycle=75.0s → T_att=67.0s
    float v_m_s = 9.6f / 3600.0f;
    check_f("t_attente nominale (67.0s)",
            67.0f, calcul_t_attente_s(0.20f, v_m_s, 3.0f, 5.0f), 0.1f);

    check_f("t_attente < 0 → clamp 0",
            0.0f,  calcul_t_attente_s(0.20f, v_m_s, 50.0f, 50.0f), 0.001f);

    check_f("t_attente dist=0 → 0",
            0.0f,  calcul_t_attente_s(0.0f, v_m_s, 3.0f, 5.0f), 0.001f);

    // --- correction_vitesse ---
    ESP_LOGI(TAG, "-- correction_vitesse --");

    // erreur=+0.4 m/h × kp=0.1 → +0.04s
    check_f("correction +0.04s (trop rapide)",
            67.04f, correction_vitesse(67.0f, 10.0f, 9.6f, 0.1f), 0.001f);

    // erreur=-1.6 m/h × kp=0.1 → -0.16s
    check_f("correction -0.16s (trop lente)",
            66.84f, correction_vitesse(67.0f, 8.0f, 9.6f, 0.1f), 0.001f);

    // Résultat < 0 → clamp 0
    check_f("correction → résultat < 0 → clamp 0",
            0.0f, correction_vitesse(0.02f, 8.0f, 9.6f, 0.1f), 0.001f);

    // --- calcul_longueur_depuis_impulsions ---
    ESP_LOGI(TAG, "-- calcul_longueur_depuis_impulsions --");

    // 120 imp → étage 1 (R=0.295m, dist_pulse=0.18535m) → 120×0.18535 = 22.242m
    check_f("longueur 120 imp",
            22.242f, calcul_longueur_depuis_impulsions(120, r0, d, 330.0f, 4), 0.05f);

    check_f("longueur 0 imp → 0m",
            0.0f, calcul_longueur_depuis_impulsions(0, r0, d, 330.0f, 4), 0.001f);

    check_f("longueur params invalides → 0m",
            0.0f, calcul_longueur_depuis_impulsions(100, 0.0f, 0.0f, 330.0f, 4), 0.001f);

    // --- calcul_correction_capteur ---
    ESP_LOGI(TAG, "-- calcul_correction_capteur --");

    check_f("k_capteur reelle=calculee → 1.0",
            1.0f,  calcul_correction_capteur(100.0f, 100.0f), 0.001f);

    check_f("k_capteur +5%",
            1.05f, calcul_correction_capteur(105.0f, 100.0f), 0.001f);

    check_f("k_capteur rejet: calculee < 5m",
            -1.0f, calcul_correction_capteur(330.0f, 2.0f), 0.001f);

    check_f("k_capteur rejet: k > 2.0",
            -1.0f, calcul_correction_capteur(330.0f, 50.0f), 0.001f);

    check_f("k_capteur rejet: calculee = 0",
            -1.0f, calcul_correction_capteur(330.0f, 0.0f), 0.001f);

    check_f("k_capteur rejet: reelle <= 0",
            -1.0f, calcul_correction_capteur(0.0f, 100.0f), 0.001f);

    ESP_LOGI(TAG, "=== Résultat : %d/%d tests OK ===", s_ok, s_total);
}
