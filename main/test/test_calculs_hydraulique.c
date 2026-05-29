#include "sdkconfig.h"
#include "calculs_hydraulique.h"
#include "abaques/abaques.h"
#include "esp_log.h"
#include <math.h>

// Tests unitaires calculs hydrauliques — PR-03
// Compilé uniquement si CONFIG_IRRI_ENABLE_TESTS=y
static const char *TAG = "test_hyd";

#define EPSILON 0.5f   // tolérance m/h (interpolation approximative)
#define EPS_SM  0.01f  // tolérance pour surface, dose (valeurs exactes)

#define PASS(name) ESP_LOGI(TAG, "PASS  %s", name)
#define FAIL(name, fmt, ...) ESP_LOGE(TAG, "FAIL  %s -- " fmt, name, ##__VA_ARGS__)

static void assert_near(float attendu, float reel, float eps, const char *nom)
{
    if (fabsf(attendu - reel) <= eps) {
        PASS(nom);
    } else {
        FAIL(nom, "attendu=%.3f obtenu=%.3f", attendu, reel);
    }
}

// =============================================================================
// Tests lookup_vitesse_cible
// =============================================================================

// Test 1 — Correspondance exacte entry 0 (p=4.9, buse=17.3), dose=25 → D25=15.3
static void test_exact_entry0_d25(void)
{
    float debit = 0.0f;
    float v = lookup_vitesse_cible(&ABAQUE_SR150C, 4.9f, 17.3f, 25.0f, &debit, NULL);
    assert_near(15.3f, v,     EPSILON, "exact_entry0_d25_vitesse");
    assert_near(23.0f, debit, EPSILON, "exact_entry0_d25_debit");
}

// Test 2 — Correspondance exacte entry 6 (p=7.7, buse=22.9), dose=20 → D20=28.1
static void test_exact_entry6_d20(void)
{
    float v = lookup_vitesse_cible(&ABAQUE_SR150C, 7.7f, 22.9f, 20.0f, NULL, NULL);
    assert_near(28.1f, v, EPSILON, "exact_entry6_d20");
}

// Test 3 — Correspondance exacte entry 11 (p=9.5, buse=20.3), dose=40 → D40=13.6
static void test_exact_entry11_d40(void)
{
    float v = lookup_vitesse_cible(&ABAQUE_SR150C, 9.5f, 20.3f, 40.0f, NULL, NULL);
    assert_near(13.6f, v, EPSILON, "exact_entry11_d40");
}

// Test 4 — Interpolation dose entre D25=15.3 et D20=19.2, dose=22.5
// t = (25-22.5)/(25-20) = 0.5 → 15.3 + 0.5*3.9 = 17.25
static void test_interp_dose(void)
{
    float v = lookup_vitesse_cible(&ABAQUE_SR150C, 4.9f, 17.3f, 22.5f, NULL, NULL);
    assert_near(17.25f, v, EPSILON, "interp_dose_22.5");
}

// Test 5 — Clamp dose > 40mm → D40 (vitesse la plus lente)
static void test_clamp_dose_haute(void)
{
    float v = lookup_vitesse_cible(&ABAQUE_SR150C, 4.9f, 17.3f, 50.0f, NULL, NULL);
    assert_near(9.6f, v, EPSILON, "clamp_dose_haute");
}

// Test 6 — Clamp dose < 15mm → D15 (vitesse la plus rapide)
static void test_clamp_dose_basse(void)
{
    float v = lookup_vitesse_cible(&ABAQUE_SR150C, 4.9f, 17.3f, 5.0f, NULL, NULL);
    assert_near(25.6f, v, EPSILON, "clamp_dose_basse");
}

// Test 7 — Nearest neighbor (p/buse décalés → proche de entry 0)
static void test_nearest_neighbor(void)
{
    float v = lookup_vitesse_cible(&ABAQUE_SR150C, 5.0f, 17.5f, 25.0f, NULL, NULL);
    // Proche de entry0 (15.3) mais légèrement pondéré vers entry1 (15.6)
    assert_near(15.3f, v, EPSILON, "nearest_neighbor");
}

// Test 8 — Interpolation entre 2 voisins non-exacts
// p=6.0, buse=20.3 → entre entry2 (5.7/20.3) et entry3 (6.5/20.3), dose=30
// ranges SR150C: p_range=4.6, buse_range=8.1
// d2 = |6.0-5.7|/4.6 = 0.065 ; d3 = |6.0-6.5|/4.6 = 0.109
// w2=1/0.065=15.38, w3=1/0.109=9.17, wt=24.55
// v = (15.38*15.8 + 9.17*16.1) / 24.55 = 15.91
static void test_interp_deux_voisins(void)
{
    float v = lookup_vitesse_cible(&ABAQUE_SR150C, 6.0f, 20.3f, 30.0f, NULL, NULL);
    // Tolérance plus large car c'est une estimation pondérée
    assert_near(15.9f, v, 0.8f, "interp_deux_voisins");
}

// Test 9 — p_buse_out : exact entry 0 → p_buse=3.5
static void test_p_buse_out(void)
{
    float p_buse = 0.0f;
    lookup_vitesse_cible(&ABAQUE_SR150C, 4.9f, 17.3f, 25.0f, NULL, &p_buse);
    assert_near(3.5f, p_buse, EPSILON, "p_buse_out_entry0");
}

// Test 10 — p_buse_out exact entry 6 → p_buse=4.0
static void test_p_buse_out_entry6(void)
{
    float p_buse = 0.0f;
    lookup_vitesse_cible(&ABAQUE_SR150C, 7.7f, 22.9f, 20.0f, NULL, &p_buse);
    assert_near(4.0f, p_buse, EPSILON, "p_buse_out_entry6");
}

// Test 11 — abaque NULL → retourne 0, ne crash pas
static void test_abaque_null(void)
{
    float debit = 99.0f, p_buse = 99.0f;
    float v = lookup_vitesse_cible(NULL, 5.0f, 20.0f, 25.0f, &debit, &p_buse);
    if (fabsf(v) < 0.01f && fabsf(debit) < 0.01f && fabsf(p_buse) < 0.01f) {
        PASS("abaque_null");
    } else {
        FAIL("abaque_null", "v=%.3f debit=%.3f p_buse=%.3f", v, debit, p_buse);
    }
}

// =============================================================================
// Tests calcul_surface_m2 et calcul_dose_inst_mm
// =============================================================================

// Test 12 — Surface 100m × 60m = 6000 m²
static void test_surface(void)
{
    float s = calcul_surface_m2(100.0f, 60.0f);
    assert_near(6000.0f, s, EPS_SM, "surface_100x60");
}

// Test 13 — Surface nulle
static void test_surface_zero(void)
{
    float s = calcul_surface_m2(0.0f, 60.0f);
    assert_near(0.0f, s, EPS_SM, "surface_zero");
}

// Test 14 — Dose instantanée : debit=25 m³/h, vitesse=15 m/h, largeur=60m
// dose = 25 / (15 × 60) × 1000 = 27.78 mm
static void test_dose_inst(void)
{
    float d = calcul_dose_inst_mm(25.0f, 15.0f, 60.0f);
    assert_near(27.78f, d, 0.1f, "dose_inst_25_15_60");
}

// Test 15 — Dose instantanée : vitesse=0 → retourne 0 (pas de division par 0)
static void test_dose_inst_vitesse_nulle(void)
{
    float d = calcul_dose_inst_mm(25.0f, 0.0f, 60.0f);
    assert_near(0.0f, d, EPS_SM, "dose_inst_vitesse_zero");
}

// Test 16 — Dose instantanée : largeur=0 → retourne 0
static void test_dose_inst_largeur_nulle(void)
{
    float d = calcul_dose_inst_mm(25.0f, 15.0f, 0.0f);
    assert_near(0.0f, d, EPS_SM, "dose_inst_largeur_zero");
}

// =============================================================================
// Point d'entrée
// =============================================================================
void test_calculs_hydraulique_run(void)
{
    ESP_LOGI(TAG, "=== Tests calculs hydraulique — PR-03 ===");

    test_exact_entry0_d25();
    test_exact_entry6_d20();
    test_exact_entry11_d40();
    test_interp_dose();
    test_clamp_dose_haute();
    test_clamp_dose_basse();
    test_nearest_neighbor();
    test_interp_deux_voisins();
    test_p_buse_out();
    test_p_buse_out_entry6();
    test_abaque_null();
    test_surface();
    test_surface_zero();
    test_dose_inst();
    test_dose_inst_vitesse_nulle();
    test_dose_inst_largeur_nulle();

    ESP_LOGI(TAG, "=== Fin tests hydraulique ===");
}
