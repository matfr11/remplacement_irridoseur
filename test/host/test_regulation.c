#include "unity.h"
#include "regulation.h"

void setUp(void) { regulation_reset_calibration(); }
void tearDown(void) {}

// 1 — T_attente nominal : dist=1m, v=0.5m/s, t_rempl=0.8s, t_vidange=0.5s → 0.7s
static void test_t_attente_nominal(void)
{
    bool alerte = false;
    float t = calcul_t_attente_s(1.0f, 0.5f, 0.8f, 0.5f, &alerte);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.7f, t);
    TEST_ASSERT_FALSE(alerte);
}

// 2 — T_attente négatif → retourne 0.0, alerte = true
static void test_t_attente_negatif(void)
{
    bool alerte = false;
    float t = calcul_t_attente_s(0.1f, 1.0f, 0.5f, 0.5f, &alerte);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, t);
    TEST_ASSERT_TRUE(alerte);
}

// 3 — correction_vitesse : v_reel < v_cible → T augmente (102s)
static void test_correction_positive(void)
{
    float t = correction_vitesse(100.0f, 8.0f, 10.0f, 0.1f);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 102.0f, t);
}

// 4 — correction_vitesse : sur-correction → clampé à 0.0
static void test_correction_clamp(void)
{
    float t = correction_vitesse(1.0f, 200.0f, 10.0f, 1.0f);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, t);
}

// 5 — calcul_cycles_par_min : v=60m/h, dist=1m → 1.0 cycle/min
static void test_cycles_par_min(void)
{
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 1.0f,  calcul_cycles_par_min(60.0f, 1.0f));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 2.0f,  calcul_cycles_par_min(60.0f, 0.5f));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f,  calcul_cycles_par_min(60.0f, 0.0f));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f,  calcul_cycles_par_min(60.0f, -1.0f));
}

void suite_regulation(void)
{
    RUN_TEST(test_t_attente_nominal);
    RUN_TEST(test_t_attente_negatif);
    RUN_TEST(test_correction_positive);
    RUN_TEST(test_correction_clamp);
    RUN_TEST(test_cycles_par_min);
}
