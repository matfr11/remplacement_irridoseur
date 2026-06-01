#include "sdkconfig.h"
#include "calculs_hydraulique.h"
#include "abaques/abaques.h"
#include "esp_log.h"
#include <math.h>

static const char *TAG = "test_hyd";

#define EPSILON 0.5f   // tolérance m/h pour vitesse et m³/h pour débit
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
// Tests lookup_vitesse_cible — formule Torricelli
//
// Q = k_q × buse² × √p_buse          (p_buse : interpolé IDW depuis l'abaque)
// V = Q × 1000 / (largeur × dose)
//
// SR 150C : k_q=0.039
// SR 100C : k_q=0.039
// =============================================================================

// Test 1 — Exact entry (p=4.9, buse=17.3) → p_buse=3.5
// Q = 0.039 × 17.3² × √3.5 = 21.84   V = 21.84×1000 / (60×25) = 14.56
static void test_sr150c_exact_buse173_d25(void)
{
    float debit = 0.0f;
    float v = lookup_vitesse_cible(&ABAQUE_SR150C, 4.9f, 17.3f, 25.0f, 60.0f, &debit, NULL);
    assert_near(14.56f, v,     EPSILON, "sr150c_buse173_d25_vitesse");
    assert_near(21.84f, debit, EPSILON, "sr150c_buse173_d25_debit");
}

// Test 2 — Exact entry (p=7.7, buse=22.9) → p_buse=4.0
// Q = 0.039 × 22.9² × √4.0 = 40.90   V = 40.90×1000 / (72×20) = 28.40
static void test_sr150c_exact_buse229_d20(void)
{
    float v = lookup_vitesse_cible(&ABAQUE_SR150C, 7.7f, 22.9f, 20.0f, 72.0f, NULL, NULL);
    assert_near(28.40f, v, EPSILON, "sr150c_buse229_d20");
}

// Test 3 — Exact entry haute pression (p=9.5, buse=20.3) → p_buse=6.0
// Q = 0.039 × 20.3² × √6.0 = 39.36   V = 39.36×1000 / (72×40) = 13.67
static void test_sr150c_exact_buse203_d40(void)
{
    float v = lookup_vitesse_cible(&ABAQUE_SR150C, 9.5f, 20.3f, 40.0f, 72.0f, NULL, NULL);
    assert_near(13.67f, v, EPSILON, "sr150c_buse203_d40");
}

// Test 4 — Dose proportionnelle inverse : même entry (4.9/17.3), dose=20 au lieu de 25
// V = 21.84×1000 / (60×20) = 18.20   (soit vitesse test1 × 25/20)
static void test_dose_prop_inverse(void)
{
    float v = lookup_vitesse_cible(&ABAQUE_SR150C, 4.9f, 17.3f, 20.0f, 60.0f, NULL, NULL);
    assert_near(18.20f, v, EPSILON, "dose_prop_inverse");
}

// Test 5 — Dose > 40mm : formule continue, pas de clamping
// V = 21.84×1000 / (60×50) = 7.28
static void test_dose_hors_plage_haute(void)
{
    float v = lookup_vitesse_cible(&ABAQUE_SR150C, 4.9f, 17.3f, 50.0f, 60.0f, NULL, NULL);
    assert_near(7.28f, v, EPSILON, "dose_hors_plage_haute");
}

// Test 6 — Dose < 15mm : formule continue
// V = 21.84×1000 / (60×10) = 36.40
static void test_dose_hors_plage_basse(void)
{
    float v = lookup_vitesse_cible(&ABAQUE_SR150C, 4.9f, 17.3f, 10.0f, 60.0f, NULL, NULL);
    assert_near(36.40f, v, EPSILON, "dose_hors_plage_basse");
}

// Test 7 — IDW : voisin dominant (4.9/17.3) depuis (5.0/17.5)
// p_range=4.6, buse_range=8.1
// d(4.9/17.3)=0.033, d(5.6/17.3)=0.133 → p_buse≈3.60
// Q = 0.039 × 17.5² × √3.60 = 22.65   V = 22.65×1000 / (60×25) = 15.10
static void test_interp_voisin_dominant(void)
{
    float v = lookup_vitesse_cible(&ABAQUE_SR150C, 5.0f, 17.5f, 25.0f, 60.0f, NULL, NULL);
    assert_near(15.10f, v, EPSILON, "interp_voisin_dominant");
}

// Test 8 — IDW entre (5.7/20.3/p_buse=3.5) et (6.5/20.3/p_buse=4.0)
// À p=6.0 buse=20.3 : d0=0.065, d1=0.109 → w0=15.4, w1=9.2 → p_buse≈3.69
// Q = 0.039 × 20.3² × √3.69 = 30.85   V = 30.85×1000 / (66×30) = 15.58
static void test_interp_deux_voisins(void)
{
    float v = lookup_vitesse_cible(&ABAQUE_SR150C, 6.0f, 20.3f, 30.0f, 66.0f, NULL, NULL);
    assert_near(15.58f, v, 0.8f, "interp_deux_voisins");
}

// Test 9 — p_buse_out : exact entry (4.9/17.3) → p_buse=3.5
static void test_p_buse_out(void)
{
    float p_buse = 0.0f;
    lookup_vitesse_cible(&ABAQUE_SR150C, 4.9f, 17.3f, 25.0f, 60.0f, NULL, &p_buse);
    assert_near(3.5f, p_buse, EPSILON, "p_buse_out_buse173");
}

// Test 10 — p_buse_out : exact entry (7.7/22.9) → p_buse=4.0
static void test_p_buse_out_buse229(void)
{
    float p_buse = 0.0f;
    lookup_vitesse_cible(&ABAQUE_SR150C, 7.7f, 22.9f, 20.0f, 72.0f, NULL, &p_buse);
    assert_near(4.0f, p_buse, EPSILON, "p_buse_out_buse229");
}

// Test 11 — abaque NULL → retourne 0, ne crash pas
static void test_abaque_null(void)
{
    float debit = 99.0f, p_buse = 99.0f;
    float v = lookup_vitesse_cible(NULL, 5.0f, 20.0f, 25.0f, 60.0f, &debit, &p_buse);
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
// Tests SR 100C
// =============================================================================

// Test SR100C-1 — Exact entry (p=4.5, buse=12.7) → p_buse=4.0
// Q = 0.039 × 12.7² × √4.0 = 12.58   V = 12.58×1000 / (48×25) = 10.48
static void test_sr100c_exact_buse127_d25(void)
{
    float debit = 0.0f;
    float v = lookup_vitesse_cible(&ABAQUE_SR100C, 4.5f, 12.7f, 25.0f, 48.0f, &debit, NULL);
    assert_near(10.48f, v,     EPSILON, "sr100c_buse127_d25_vitesse");
    assert_near(12.58f, debit, EPSILON, "sr100c_buse127_d25_debit");
}

// Test SR100C-2 — Exact entry (p=7.4, buse=19.0) → p_buse=5.0
// Q = 0.039 × 19.0² × √5.0 = 31.48   V = 31.48×1000 / (66×30) = 15.90
static void test_sr100c_exact_buse190_d30(void)
{
    float v = lookup_vitesse_cible(&ABAQUE_SR100C, 7.4f, 19.0f, 30.0f, 66.0f, NULL, NULL);
    assert_near(15.90f, v, EPSILON, "sr100c_buse190_d30");
}

// Test SR100C-3 — IDW équidistant entre (4.9/15.2/p_buse=4.0) et (6.1/15.2/p_buse=5.0)
// À p=5.5 buse=15.2 : d0=d1=0.12 → p_buse = (4.0+5.0)/2 = 4.50
// Q = 0.039 × 15.2² × √4.5 = 19.11   V = 19.11×1000 / (60×25) = 12.74
static void test_sr100c_interp_buse152(void)
{
    float v = lookup_vitesse_cible(&ABAQUE_SR100C, 5.5f, 15.2f, 25.0f, 60.0f, NULL, NULL);
    assert_near(12.74f, v, EPSILON, "sr100c_interp_buse152_d25");
}

// =============================================================================
// Point d'entrée
// =============================================================================
void test_calculs_hydraulique_run(void)
{
    ESP_LOGI(TAG, "=== Tests calculs hydraulique ===");

    test_sr150c_exact_buse173_d25();
    test_sr150c_exact_buse229_d20();
    test_sr150c_exact_buse203_d40();
    test_dose_prop_inverse();
    test_dose_hors_plage_haute();
    test_dose_hors_plage_basse();
    test_interp_voisin_dominant();
    test_interp_deux_voisins();
    test_p_buse_out();
    test_p_buse_out_buse229();
    test_abaque_null();
    test_surface();
    test_surface_zero();
    test_dose_inst();
    test_dose_inst_vitesse_nulle();
    test_dose_inst_largeur_nulle();

    test_sr100c_exact_buse127_d25();
    test_sr100c_exact_buse190_d30();
    test_sr100c_interp_buse152();

    ESP_LOGI(TAG, "=== Fin tests hydraulique ===");
}
