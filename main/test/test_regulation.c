#include "regulation.h"
#include "esp_log.h"
#include <math.h>

// Tests unitaires regulation — PR-06
static const char *TAG = "test_reg";

#define PASS(name)  ESP_LOGI(TAG, "PASS  " name)
#define FAIL(name, fmt, ...) ESP_LOGE(TAG, "FAIL  " name " — " fmt, ##__VA_ARGS__)

#define TOL  1e-3f

static void assert_near(float expected, float actual, float eps, const char *name)
{
    if (fabsf(expected - actual) <= eps) {
        PASS(name);
    } else {
        FAIL(name, "expected=%.4f got=%.4f", expected, actual);
    }
}

// =============================================================================
// Test 1 — T_attente nominal
// dist=1.0m, v_cible=0.5m/s, t_rempl=0.8s, t_vidange=0.5s
// T_att = 1.0/0.5 - 0.8 - 0.5 = 0.7s
// =============================================================================
static void test_t_attente_nominal(void)
{
    bool alerte = false;
    float t = calcul_t_attente_s(1.0f, 0.5f, 0.8f, 0.5f, &alerte);
    assert_near(0.7f, t, TOL, "T_attente nominal");
    if (!alerte) {
        PASS("T_attente nominal — alerte=false");
    } else {
        FAIL("T_attente nominal — alerte=false", "alerte inattendue");
    }
}

// =============================================================================
// Test 2 — T_attente < 0 : retourne 0.0, alerte = true
// dist=0.1m, v_cible=1.0m/s, t_rempl=0.5s, t_vidange=0.5s
// T_att = 0.1 - 0.5 - 0.5 = -0.9 → 0.0
// =============================================================================
static void test_t_attente_negatif(void)
{
    bool alerte = false;
    float t = calcul_t_attente_s(0.1f, 1.0f, 0.5f, 0.5f, &alerte);
    assert_near(0.0f, t, TOL, "T_attente negatif retourne 0");
    if (alerte) {
        PASS("T_attente negatif — alerte=true");
    } else {
        FAIL("T_attente negatif — alerte=true", "alerte manquante");
    }
}

// =============================================================================
// Test 3 — T_attente > 300s : valeur retournée (log warning seulement)
// dist=300.0m, v_cible=1e-3m/s, t_rempl=0s, t_vidange=0s
// T_att = 300000s > 300 — pas de clamp, valeur retournée
// =============================================================================
static void test_t_attente_grand(void)
{
    bool alerte = false;
    float t = calcul_t_attente_s(300.0f, 1e-3f, 0.0f, 0.0f, &alerte);
    if (t > 300.0f && !alerte) {
        PASS("T_attente >300s retourne valeur, pas d'alerte");
    } else {
        FAIL("T_attente >300s retourne valeur, pas d'alerte",
             "t=%.1f alerte=%d", t, (int)alerte);
    }
}

// =============================================================================
// Test 4 — Inputs nuls (dist=0) : retourne 0.0, pas de crash
// =============================================================================
static void test_t_attente_inputs_nuls(void)
{
    bool alerte = false;
    float t = calcul_t_attente_s(0.0f, 0.5f, 0.0f, 0.0f, &alerte);
    assert_near(0.0f, t, TOL, "T_attente dist=0 retourne 0");
    t = calcul_t_attente_s(1.0f, 0.0f, 0.0f, 0.0f, &alerte);
    assert_near(0.0f, t, TOL, "T_attente v_cible=0 retourne 0");
}

// =============================================================================
// Test 5 — correction_vitesse : v_reel < v_cible → T_att augmente
// t_att=100s, v_reel=8m/h, v_cible=10m/h, kp=0.1
// correction = 0.1 × (2/10) × 100 = 2.0 → t_corrige = 102.0s
// =============================================================================
static void test_correction_positive(void)
{
    float t = correction_vitesse(100.0f, 8.0f, 10.0f, 0.1f);
    assert_near(102.0f, t, TOL, "correction_vitesse positive");
}

// =============================================================================
// Test 6 — correction_vitesse : sur-correction → clampé à 0.0
// t_att=1.0s, v_reel=200m/h, v_cible=10m/h, kp=1.0
// correction = 1.0 × (-190/10) × 1.0 = -19.0 → 1.0-19.0 = -18.0 → 0.0
// =============================================================================
static void test_correction_clamp(void)
{
    float t = correction_vitesse(1.0f, 200.0f, 10.0f, 1.0f);
    assert_near(0.0f, t, TOL, "correction_vitesse sur-correction clamp 0");
}

// =============================================================================
// Test 7 — correction_vitesse : v_reel > v_cible → T_att diminue
// t_att=100s, v_reel=12m/h, v_cible=10m/h, kp=0.1
// correction = 0.1 × (-2/10) × 100 = -2.0 → t_corrige = 98.0s
// =============================================================================
static void test_correction_negative(void)
{
    float t = correction_vitesse(100.0f, 12.0f, 10.0f, 0.1f);
    assert_near(98.0f, t, TOL, "correction_vitesse negative");
}

// =============================================================================
// Test 8 — correction_vitesse : v_cible = 0 → retourne t_attente inchangé
// =============================================================================
static void test_correction_vcible_nulle(void)
{
    float t = correction_vitesse(50.0f, 10.0f, 0.0f, 0.1f);
    assert_near(50.0f, t, TOL, "correction_vitesse v_cible=0 inchange");
}

// =============================================================================
// Test 9 — regulation_update_dist_par_cycle : moyenne glissante sur 5
// Pousser 6 valeurs [1,2,3,4,5,6] — fenêtre retient [2,3,4,5,6] → moy = 4.0
// =============================================================================
static void test_rolling_average(void)
{
    regulation_reset_calibration();
    float moy = 0.0f;
    for (int i = 1; i <= 6; i++) {
        moy = regulation_update_dist_par_cycle((float)i);
    }
    assert_near(4.0f, moy, TOL, "rolling average 6 valeurs");
}

// =============================================================================
// Test 10 — regulation_get_nb_cycles + regulation_reset_calibration
// Après 4 updates : nb_cycles = 4. Après reset : nb_cycles = 0.
// =============================================================================
static void test_cycles_et_reset(void)
{
    regulation_reset_calibration();
    regulation_update_dist_par_cycle(1.0f);
    regulation_update_dist_par_cycle(1.0f);
    regulation_update_dist_par_cycle(1.0f);
    regulation_update_dist_par_cycle(1.0f);
    int n = regulation_get_nb_cycles();
    if (n == 4) {
        PASS("nb_cycles = 4 apres 4 updates");
    } else {
        FAIL("nb_cycles = 4 apres 4 updates", "got %d", n);
    }
    regulation_reset_calibration();
    n = regulation_get_nb_cycles();
    if (n == 0) {
        PASS("nb_cycles = 0 apres reset");
    } else {
        FAIL("nb_cycles = 0 apres reset", "got %d", n);
    }
}

// =============================================================================

void test_regulation_run(void)
{
    ESP_LOGI(TAG, "=== Tests regulation (PR-06) ===");
    test_t_attente_nominal();
    test_t_attente_negatif();
    test_t_attente_grand();
    test_t_attente_inputs_nuls();
    test_correction_positive();
    test_correction_clamp();
    test_correction_negative();
    test_correction_vcible_nulle();
    test_rolling_average();
    test_cycles_et_reset();
    ESP_LOGI(TAG, "=== Fin tests regulation ===");
}
