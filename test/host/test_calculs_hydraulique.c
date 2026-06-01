#include "unity.h"
#include "unity_suite.h"
#include "calculs_hydraulique.h"
#include "abaques/abaques.h"
#include <math.h>

// Formule : Q = k_q * buse^2 * sqrt(p_buse), V = Q*1000 / (larg * dose)
// SR 150C : k_q=0.039, k_portee=7.06, portee_exp_buse=0.557, portee_exp_p=0.30, esp_factor=1.55

// 1 — entree exacte entry0 (p=4.9, buse=17.3 -> p_buse=3.5), dose=25, larg=60
//     Q = 0.039 * 299.29 * sqrt(3.5) = 21.84 m3/h
//     V = 21840 / (60*25) = 14.56 m/h
static void test_exact_entry0_d25(void)
{
    float debit = 0.0f;
    float v = lookup_vitesse_cible(&ABAQUE_SR150C, 4.9f, 17.3f, 25.0f, 60.0f, &debit, NULL);
    TEST_ASSERT_FLOAT_WITHIN(0.5f, 14.56f, v);
    TEST_ASSERT_FLOAT_WITHIN(1.0f, 21.84f, debit);
}

// 2 — entree exacte entry6 (p=7.7, buse=22.9 -> p_buse=4.0), dose=20, larg=72
//     Q = 0.039 * 524.41 * sqrt(4.0) = 40.90 m3/h
//     V = 40900 / (72*20) = 28.40 m/h
static void test_exact_entry6_d20(void)
{
    float v = lookup_vitesse_cible(&ABAQUE_SR150C, 7.7f, 22.9f, 20.0f, 72.0f, NULL, NULL);
    TEST_ASSERT_FLOAT_WITHIN(0.5f, 28.40f, v);
}

// 3 — formule continue : dose=22.5, larg=60
//     V = 21840 / (60*22.5) = 16.18 m/h
static void test_dose_continu(void)
{
    float v = lookup_vitesse_cible(&ABAQUE_SR150C, 4.9f, 17.3f, 22.5f, 60.0f, NULL, NULL);
    TEST_ASSERT_FLOAT_WITHIN(0.5f, 16.18f, v);
}

// 4 — dose=50 (hors plage, formule libre — pas de clamping)
//     V = 21840 / (60*50) = 7.28 m/h
static void test_dose_haute_libre(void)
{
    float v = lookup_vitesse_cible(&ABAQUE_SR150C, 4.9f, 17.3f, 50.0f, 60.0f, NULL, NULL);
    TEST_ASSERT_FLOAT_WITHIN(0.5f, 7.28f, v);
}

// 5 — dose=5 (hors plage, formule libre — pas de clamping)
//     V = 21840 / (60*5) = 72.8 m/h
static void test_dose_basse_libre(void)
{
    float v = lookup_vitesse_cible(&ABAQUE_SR150C, 4.9f, 17.3f, 5.0f, 60.0f, NULL, NULL);
    TEST_ASSERT_FLOAT_WITHIN(2.0f, 72.8f, v);
}

// 6 — abaque NULL -> retourne 0.0, pas de crash
static void test_abaque_null(void)
{
    float debit = 99.0f, p_buse = 99.0f;
    float v = lookup_vitesse_cible(NULL, 5.0f, 20.0f, 25.0f, 60.0f, &debit, &p_buse);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.0f, v);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.0f, debit);
}

// 7 — largeur_m=0 -> erreur obligatoire (return 0)
static void test_largeur_zero_erreur(void)
{
    float v = lookup_vitesse_cible(&ABAQUE_SR150C, 4.9f, 17.3f, 25.0f, 0.0f, NULL, NULL);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.0f, v);
}

// 8 — calcul_surface_m2 : 100m x 60m = 6000 m2
static void test_surface(void)
{
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 6000.0f, calcul_surface_m2(100.0f, 60.0f));
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.0f,    calcul_surface_m2(0.0f, 60.0f));
}

// 9 — calcul_dose_inst_mm : debit=25, v=15, larg=60 -> 27.78 mm
static void test_dose_inst(void)
{
    TEST_ASSERT_FLOAT_WITHIN(0.1f,  27.78f, calcul_dose_inst_mm(25.0f, 15.0f, 60.0f));
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.0f,   calcul_dose_inst_mm(25.0f, 0.0f,  60.0f));
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.0f,   calcul_dose_inst_mm(25.0f, 15.0f, 0.0f));
}

// 10 — correction largeur : meme Q (entry0), doses differentes largeurs
//      v60 = 14.56, v30 = 29.12 (x2), v120 = 7.28 (x0.5)
static void test_correction_largeur(void)
{
    float v60 = lookup_vitesse_cible(&ABAQUE_SR150C, 4.9f, 17.3f, 25.0f, 60.0f, NULL, NULL);
    TEST_ASSERT_FLOAT_WITHIN(0.5f, 14.56f, v60);

    float v30 = lookup_vitesse_cible(&ABAQUE_SR150C, 4.9f, 17.3f, 25.0f, 30.0f, NULL, NULL);
    TEST_ASSERT_FLOAT_WITHIN(1.0f, 29.12f, v30);
    // Ratio exact : v30 = 2 * v60
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 2.0f, v30 / v60);

    float v120 = lookup_vitesse_cible(&ABAQUE_SR150C, 4.9f, 17.3f, 25.0f, 120.0f, NULL, NULL);
    TEST_ASSERT_FLOAT_WITHIN(0.5f, 7.28f, v120);
    // Ratio exact : v120 = 0.5 * v60
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.5f, v120 / v60);
}

void suite_calculs_hydraulique(void)
{
    unity_suite_setup(NULL, NULL);
    RUN_TEST(test_exact_entry0_d25);
    RUN_TEST(test_exact_entry6_d20);
    RUN_TEST(test_dose_continu);
    RUN_TEST(test_dose_haute_libre);
    RUN_TEST(test_dose_basse_libre);
    RUN_TEST(test_abaque_null);
    RUN_TEST(test_largeur_zero_erreur);
    RUN_TEST(test_surface);
    RUN_TEST(test_dose_inst);
    RUN_TEST(test_correction_largeur);
}
