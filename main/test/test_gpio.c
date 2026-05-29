#include "sdkconfig.h"
#include "gpio_handler.h"
#include "esp_log.h"
#include <math.h>

// Tests unitaires GPIO handler — PR-02
// Exécutés au démarrage quand CONFIG_IRRI_ENABLE_TESTS=y (hors production)
static const char *TAG = "test_gpio";

#define PASS(name)  ESP_LOGI(TAG, "PASS  %s", name)
#define FAIL(name, fmt, ...) ESP_LOGE(TAG, "FAIL  %s -- " fmt, name, ##__VA_ARGS__)

// Tolérance pour comparaisons float (m/h)
#define TOL_MH  0.5f

// Injecte N pulses équidistants séparés de interval_us microsecondes
static void injecter_pulses(int n, int64_t interval_us)
{
    int64_t t = 0;
    for (int i = 0; i < n; i++) {
        gpio_handler_test_injecter_pulse(t);
        t += interval_us;
    }
}

// =============================================================================
// Test 1 — Calcul vitesse nominal
// 5 pulses espacés de 1s, dist_pulse = 1.0m, facteur = 1.0
// dt = 4s (entre pulse[0]=0 et pulse[4]=4s)
// vitesse_m_h = 5 × 1.0 × 1.0 / 4.0 × 3600 = 4500.0
// =============================================================================
static void test_vitesse_nominale(void)
{
    gpio_handler_test_reset();
    gpio_handler_set_params(5, 15);
    gpio_handler_set_dist_pulse_m(1.0f);
    injecter_pulses(5, 1000000LL);  // 5 pulses à 1s

    float v = gpio_get_vitesse_m_h(1.0f);
    float expected = 5.0f * 1.0f * 1.0f / 4.0f * 3600.0f;  // 4500.0

    if (fabsf(v - expected) < TOL_MH) {
        PASS("vitesse_nominale");
    } else {
        FAIL("vitesse_nominale", "attendu=%.1f obtenu=%.1f", expected, v);
    }
}

// =============================================================================
// Test 2 — Facteur de correction
// Même injection, facteur = 0.5 → vitesse divisée par 2
// =============================================================================
static void test_facteur_correction(void)
{
    gpio_handler_test_reset();
    gpio_handler_set_params(5, 15);
    gpio_handler_set_dist_pulse_m(1.0f);
    injecter_pulses(5, 1000000LL);

    float v = gpio_get_vitesse_m_h(0.5f);
    float expected = 5.0f * 1.0f * 0.5f / 4.0f * 3600.0f;  // 2250.0

    if (fabsf(v - expected) < TOL_MH) {
        PASS("facteur_correction");
    } else {
        FAIL("facteur_correction", "attendu=%.1f obtenu=%.1f", expected, v);
    }
}

// =============================================================================
// Test 3 — Vitesse nulle si dist_pulse non initialisée
// =============================================================================
static void test_vitesse_sans_dist_pulse(void)
{
    gpio_handler_test_reset();
    gpio_handler_set_params(5, 15);
    // dist_pulse_m pas initialisée (= 0.0)
    injecter_pulses(5, 1000000LL);

    float v = gpio_get_vitesse_m_h(1.0f);
    if (fabsf(v) < TOL_MH) {
        PASS("vitesse_sans_dist_pulse");
    } else {
        FAIL("vitesse_sans_dist_pulse", "attendu=0.0 obtenu=%.1f", v);
    }
}

// =============================================================================
// Test 4 — Vitesse nulle si fenêtre < 2 pulses
// =============================================================================
static void test_fenetre_insuffisante(void)
{
    gpio_handler_test_reset();
    gpio_handler_set_params(5, 15);
    gpio_handler_set_dist_pulse_m(1.0f);
    injecter_pulses(1, 1000000LL);  // un seul pulse

    float v = gpio_get_vitesse_m_h(1.0f);
    if (fabsf(v) < TOL_MH) {
        PASS("fenetre_insuffisante");
    } else {
        FAIL("fenetre_insuffisante", "attendu=0.0 obtenu=%.1f", v);
    }
}

// =============================================================================
// Test 5 — Vitesse nulle après timeout (cycles_sans_impulsion > max)
// =============================================================================
static void test_timeout_cycles(void)
{
    gpio_handler_test_reset();
    gpio_handler_set_params(5, 3);  // max = 3 cycles
    gpio_handler_set_dist_pulse_m(1.0f);
    injecter_pulses(5, 1000000LL);

    // Vérifier que la vitesse est valide avant le timeout
    float v_avant = gpio_get_vitesse_m_h(1.0f);
    if (fabsf(v_avant) < TOL_MH) {
        FAIL("timeout_cycles", "vitesse avant timeout attendue non-nulle, obtenu=%.1f", v_avant);
        return;
    }

    // Simuler 4 ticks sans impulsion (> max=3)
    for (int i = 0; i < 4; i++) {
        gpio_handler_tick_cycle();
    }

    float v_apres = gpio_get_vitesse_m_h(1.0f);
    if (fabsf(v_apres) < TOL_MH) {
        PASS("timeout_cycles");
    } else {
        FAIL("timeout_cycles", "attendu=0.0 apres timeout, obtenu=%.1f", v_apres);
    }
}

// =============================================================================
// Test 6 — Mode dégradé A — retourne vitesse estimée injectée
// =============================================================================
static void test_mode_degrade_a(void)
{
    gpio_handler_test_reset();
    gpio_handler_set_params(5, 15);
    // Pas de pulses, pas de dist_pulse — mode A fournit la vitesse
    float vitesse_estimee = 45.7f;
    gpio_handler_set_mode_degrade_a(true);
    gpio_handler_set_vitesse_estimee(vitesse_estimee);

    float v = gpio_get_vitesse_m_h(1.0f);
    if (fabsf(v - vitesse_estimee) < TOL_MH) {
        PASS("mode_degrade_a");
    } else {
        FAIL("mode_degrade_a", "attendu=%.1f obtenu=%.1f", vitesse_estimee, v);
    }
}

// =============================================================================
// Test 7 — Mode dégradé A désactivé — retour au calcul ISR
// =============================================================================
static void test_mode_degrade_a_desactive(void)
{
    gpio_handler_test_reset();
    gpio_handler_set_params(5, 15);
    gpio_handler_set_dist_pulse_m(1.0f);
    gpio_handler_set_mode_degrade_a(true);
    gpio_handler_set_vitesse_estimee(999.0f);

    // Désactiver le mode A et injecter des pulses réels
    gpio_handler_set_mode_degrade_a(false);
    injecter_pulses(5, 1000000LL);

    float v = gpio_get_vitesse_m_h(1.0f);
    float expected = 5.0f * 1.0f * 1.0f / 4.0f * 3600.0f;  // 4500.0

    if (fabsf(v - expected) < TOL_MH) {
        PASS("mode_degrade_a_desactive");
    } else {
        FAIL("mode_degrade_a_desactive", "attendu=%.1f obtenu=%.1f (mode A actif?)", expected, v);
    }
}

// =============================================================================
// Test 8 — Compteur impulsions et reset
// =============================================================================
static void test_compteur_impulsions(void)
{
    gpio_handler_test_reset();
    injecter_pulses(7, 500000LL);

    uint32_t count = gpio_get_impulsions();
    if (count == 7) {
        PASS("compteur_impulsions_7");
    } else {
        FAIL("compteur_impulsions_7", "attendu=7 obtenu=%lu", (unsigned long)count);
    }

    gpio_reset_impulsions_cycle();
    count = gpio_get_impulsions();
    if (count == 0) {
        PASS("reset_impulsions");
    } else {
        FAIL("reset_impulsions", "attendu=0 obtenu=%lu", (unsigned long)count);
    }
}

// =============================================================================
// Test 9 — Lecture entrées (smoke test — valeurs dépendent du câblage réel)
// =============================================================================
static void test_lire_entrees(void)
{
    entrees_t e;
    gpio_handler_lire_entrees(&e);
    // Vérification que l'appel ne crash pas (struct bien remplie)
    // Les valeurs dépendent du matériel — on ne peut pas asserter ici
    ESP_LOGI(TAG, "lire_entrees: fin_course=%d secu_spires=%d poumon=%d pression=%d",
             e.fin_course, e.secu_spires, e.poumon_plein, e.pression_ok);
    PASS("lire_entrees_no_crash");
}

// =============================================================================
// Point d'entrée
// =============================================================================
void test_gpio_run(void)
{
    ESP_LOGI(TAG, "=== Tests GPIO — PR-02 ===");

    test_vitesse_nominale();
    test_facteur_correction();
    test_vitesse_sans_dist_pulse();
    test_fenetre_insuffisante();
    test_timeout_cycles();
    test_mode_degrade_a();
    test_mode_degrade_a_desactive();
    test_compteur_impulsions();
    test_lire_entrees();

    ESP_LOGI(TAG, "=== Fin tests GPIO ===");
}
