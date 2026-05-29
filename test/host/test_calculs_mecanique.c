#include "unity.h"
#include "calculs_mecanique.h"
#include "machines/machines.h"
#include <math.h>

void setUp(void) {}
void tearDown(void) {}

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

// 6 — étage courant : 0m → étage 1, 70m → étage 2, 330m (>géo) → étage 4
static void test_etage_courant(void)
{
    s_profil = MACHINE_ST1BIS_82_330;
    machine_resoudre_double_entree(&s_profil);
    TEST_ASSERT_EQUAL_INT(1, calcul_etage_courant(0.0f,   &s_profil));
    TEST_ASSERT_EQUAL_INT(2, calcul_etage_courant(70.0f,  &s_profil));
    TEST_ASSERT_EQUAL_INT(4, calcul_etage_courant(330.0f, &s_profil));
}

// 7 — étalonnage : facteur hors plage (3.0) refusé, facteur 1.05 accepté
static void test_etalonnage(void)
{
    float facteur = 0.0f;
    TEST_ASSERT_FALSE(calcul_facteur_etalonnage(100.0f, 300.0f, 100, &facteur));
    TEST_ASSERT_TRUE(calcul_facteur_etalonnage(100.0f, 105.0f, 100, &facteur));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 1.05f, facteur);
}

void suite_calculs_mecanique(void)
{
    RUN_TEST(test_rayon_etage4);
    RUN_TEST(test_rayon_etage1);
    RUN_TEST(test_largeur_bobine_auto);
    RUN_TEST(test_dist_pulse);
    RUN_TEST(test_longueur_etage1);
    RUN_TEST(test_etage_courant);
    RUN_TEST(test_etalonnage);
}
