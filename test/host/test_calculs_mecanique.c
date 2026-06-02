#include "unity.h"
#include "unity_suite.h"
#include "calculs_mecanique.h"
#include "machines/machines.h"
#include <math.h>

static machine_profile_t s_profil;

// 1 — rayon étage 4 : 0.690 + 3.5 × 0.082 = 0.977m
static void test_rayon_etage4(void)
{
    s_profil = MACHINE_ST1BIS_82_330;
    machine_resoudre_double_entree(&s_profil);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.977f, calcul_rayon_etage(4, &s_profil));
}

// 2 — rayon étage 1 : 0.690 + 0.5 × 0.082 = 0.731m
static void test_rayon_etage1(void)
{
    s_profil = MACHINE_ST1BIS_82_330;
    machine_resoudre_double_entree(&s_profil);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.731f, calcul_rayon_etage(1, &s_profil));
}

// 3 — largeur bobine auto-calculée ≈ 1.103m
static void test_largeur_bobine_auto(void)
{
    s_profil = MACHINE_ST1BIS_82_330;
    machine_resoudre_double_entree(&s_profil);
    TEST_ASSERT_FLOAT_WITHIN(0.005f, 1.103f, s_profil.largeur_bobine_m);
}

// 4 — dist_pulse étage 4 : 2π × 0.977 / 10 ≈ 0.6139m
static void test_dist_pulse(void)
{
    s_profil = MACHINE_ST1BIS_82_330;
    machine_resoudre_double_entree(&s_profil);
    float r4 = calcul_rayon_etage(4, &s_profil);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.6139f, calcul_dist_pulse_m(r4));
}

// 5 — longueur étage 1 : 13.45 × 2π × 0.731 ≈ 61.78m
static void test_longueur_etage1(void)
{
    s_profil = MACHINE_ST1BIS_82_330;
    machine_resoudre_double_entree(&s_profil);
    TEST_ASSERT_FLOAT_WITHIN(0.1f, 61.78f, calcul_longueur_etage_m(1, &s_profil));
}

// 6 — étage courant : 0m→1, 70m→2, 295m→5, 330m→5
//     cumuls : 61.8 / 130.5 / 206.1 / 288.7 / 328.6m (dernier étage = 6 spires)
static void test_etage_courant(void)
{
    s_profil = MACHINE_ST1BIS_82_330;
    machine_resoudre_double_entree(&s_profil);
    TEST_ASSERT_EQUAL_INT(1, calcul_etage_courant(0.0f,   &s_profil));
    TEST_ASSERT_EQUAL_INT(2, calcul_etage_courant(70.0f,  &s_profil));
    TEST_ASSERT_EQUAL_INT(5, calcul_etage_courant(295.0f, &s_profil));
    TEST_ASSERT_EQUAL_INT(5, calcul_etage_courant(330.0f, &s_profil));
}

// 7 — étalonnage : facteur hors plage (3.0) refusé, facteur 1.05 accepté
static void test_etalonnage(void)
{
    float facteur = 0.0f;
    TEST_ASSERT_FALSE(calcul_facteur_etalonnage(100.0f, 300.0f, 100, &facteur));
    TEST_ASSERT_TRUE(calcul_facteur_etalonnage(100.0f, 105.0f, 100, &facteur));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 1.05f, facteur);
}

// 8 — rayon étage 5 : 0.690 + 4.5 × 0.082 = 1.059m
static void test_rayon_etage5(void)
{
    s_profil = MACHINE_ST1BIS_82_330;
    machine_resoudre_double_entree(&s_profil);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 1.059f, calcul_rayon_etage(5, &s_profil));
}

// 9 — longueur étage 5 (dernier, 6 spires) : 6 × 2π × 1.059 ≈ 39.92m
static void test_longueur_dernier_etage(void)
{
    s_profil = MACHINE_ST1BIS_82_330;
    machine_resoudre_double_entree(&s_profil);
    TEST_ASSERT_FLOAT_WITHIN(0.1f, 39.92f, calcul_longueur_etage_m(5, &s_profil));
}

void suite_calculs_mecanique(void)
{
    unity_suite_setup(NULL, NULL);
    RUN_TEST(test_rayon_etage4);
    RUN_TEST(test_rayon_etage1);
    RUN_TEST(test_largeur_bobine_auto);
    RUN_TEST(test_dist_pulse);
    RUN_TEST(test_longueur_etage1);
    RUN_TEST(test_etage_courant);
    RUN_TEST(test_etalonnage);
    RUN_TEST(test_rayon_etage5);
    RUN_TEST(test_longueur_dernier_etage);
}
