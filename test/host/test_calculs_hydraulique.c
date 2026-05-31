#include "unity.h"
#include "unity_suite.h"
#include "calculs_hydraulique.h"
#include "abaques/abaques.h"
#include <math.h>

// 1 — entrée exacte entry0 (p=4.9, buse=17.3), dose=25, largeur=0 → 15.3 m/h (compat)
static void test_exact_entry0_d25(void)
{
    float debit = 0.0f;
    float v = lookup_vitesse_cible(&ABAQUE_SR150C, 4.9f, 17.3f, 25.0f, 0.0f, &debit, NULL);
    TEST_ASSERT_FLOAT_WITHIN(0.5f, 15.3f, v);
    TEST_ASSERT_FLOAT_WITHIN(0.5f, 23.0f, debit);
}

// 2 — entrée exacte entry6 (p=7.7, buse=22.9), dose=20 → 28.1 m/h
static void test_exact_entry6_d20(void)
{
    float v = lookup_vitesse_cible(&ABAQUE_SR150C, 7.7f, 22.9f, 20.0f, 0.0f, NULL, NULL);
    TEST_ASSERT_FLOAT_WITHIN(0.5f, 28.1f, v);
}

// 3 — interpolation dose entre D25=15.3 et D20=19.2, dose=22.5 → ~17.25 m/h
static void test_interp_dose(void)
{
    float v = lookup_vitesse_cible(&ABAQUE_SR150C, 4.9f, 17.3f, 22.5f, 0.0f, NULL, NULL);
    TEST_ASSERT_FLOAT_WITHIN(0.5f, 17.25f, v);
}

// 4 — clamp dose > 40mm → D40 (vitesse la plus lente)
static void test_clamp_dose_haute(void)
{
    float v = lookup_vitesse_cible(&ABAQUE_SR150C, 4.9f, 17.3f, 50.0f, 0.0f, NULL, NULL);
    TEST_ASSERT_FLOAT_WITHIN(0.5f, 9.6f, v);
}

// 5 — clamp dose < 15mm → D15 (vitesse la plus rapide)
static void test_clamp_dose_basse(void)
{
    float v = lookup_vitesse_cible(&ABAQUE_SR150C, 4.9f, 17.3f, 5.0f, 0.0f, NULL, NULL);
    TEST_ASSERT_FLOAT_WITHIN(0.5f, 25.6f, v);
}

// 6 — abaque NULL → retourne 0.0, pas de crash
static void test_abaque_null(void)
{
    float debit = 99.0f, p_buse = 99.0f;
    float v = lookup_vitesse_cible(NULL, 5.0f, 20.0f, 25.0f, 0.0f, &debit, &p_buse);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.0f, v);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.0f, debit);
}

// 7 — calcul_surface_m2 : 100m × 60m = 6000 m²
static void test_surface(void)
{
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 6000.0f, calcul_surface_m2(100.0f, 60.0f));
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.0f,    calcul_surface_m2(0.0f, 60.0f));
}

// 8 — calcul_dose_inst_mm : debit=25, v=15, larg=60 → 27.78 mm; v=0 → 0
static void test_dose_inst(void)
{
    TEST_ASSERT_FLOAT_WITHIN(0.1f,  27.78f, calcul_dose_inst_mm(25.0f, 15.0f, 60.0f));
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.0f,   calcul_dose_inst_mm(25.0f, 0.0f,  60.0f));
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.0f,   calcul_dose_inst_mm(25.0f, 15.0f, 0.0f));
}

// 9 — correction largeur : entry0 esp=60m, largeur=30m → v × (60/30) = 2× plus rapide
// Vérification : v = Q×1000/(dose×largeur) = 23.0×1000/(25×30) = 30.67 m/h
static void test_correction_largeur(void)
{
    // Avec largeur=60 (esp de référence) → 15.3 m/h
    float v60 = lookup_vitesse_cible(&ABAQUE_SR150C, 4.9f, 17.3f, 25.0f, 60.0f, NULL, NULL);
    TEST_ASSERT_FLOAT_WITHIN(0.5f, 15.3f, v60);

    // Avec largeur=30 (moitié) → vitesse double ≈ 30.6 m/h
    float v30 = lookup_vitesse_cible(&ABAQUE_SR150C, 4.9f, 17.3f, 25.0f, 30.0f, NULL, NULL);
    TEST_ASSERT_FLOAT_WITHIN(1.0f, 30.6f, v30);

    // Avec largeur=120 (double) → vitesse moitié ≈ 7.65 m/h
    float v120 = lookup_vitesse_cible(&ABAQUE_SR150C, 4.9f, 17.3f, 25.0f, 120.0f, NULL, NULL);
    TEST_ASSERT_FLOAT_WITHIN(0.5f, 7.65f, v120);
}

void suite_calculs_hydraulique(void)
{
    unity_suite_setup(NULL, NULL);
    RUN_TEST(test_exact_entry0_d25);
    RUN_TEST(test_exact_entry6_d20);
    RUN_TEST(test_interp_dose);
    RUN_TEST(test_clamp_dose_haute);
    RUN_TEST(test_clamp_dose_basse);
    RUN_TEST(test_abaque_null);
    RUN_TEST(test_surface);
    RUN_TEST(test_dose_inst);
    RUN_TEST(test_correction_largeur);
}
