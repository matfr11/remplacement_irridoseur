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

// 11 — SR 100C exact entry (p=4.5, buse=12.7 -> p_buse=4.0), dose=25, larg=48
//      Q = 0.039 * 161.29 * sqrt(4.0) = 12.58 m3/h
//      V = 12580 / (48*25) = 10.48 m/h
static void test_sr100c_exact_buse127_d25(void)
{
    float debit = 0.0f;
    float v = lookup_vitesse_cible(&ABAQUE_SR100C, 4.5f, 12.7f, 25.0f, 48.0f, &debit, NULL);
    TEST_ASSERT_FLOAT_WITHIN(0.5f, 10.48f, v);
    TEST_ASSERT_FLOAT_WITHIN(1.0f, 12.58f, debit);
}

// 12 — SR 100C exact entry (p=7.4, buse=19.0 -> p_buse=5.0), dose=30, larg=66
//      Q = 0.039 * 361.0 * sqrt(5.0) = 31.48 m3/h
//      V = 31480 / (66*30) = 15.90 m/h
static void test_sr100c_exact_buse190_d30(void)
{
    float v = lookup_vitesse_cible(&ABAQUE_SR100C, 7.4f, 19.0f, 30.0f, 66.0f, NULL, NULL);
    TEST_ASSERT_FLOAT_WITHIN(0.5f, 15.90f, v);
}

// 13 — SR 100C IDW équidistant entre (4.9/15.2/p_buse=4.0) et (6.1/15.2/p_buse=5.0)
//      p=5.5, d0=d1=0.12 -> p_buse=4.50
//      Q = 0.039 * 231.04 * sqrt(4.5) = 19.11   V = 19110/(60*25) = 12.74 m/h
static void test_sr100c_interp_buse152(void)
{
    float v = lookup_vitesse_cible(&ABAQUE_SR100C, 5.5f, 15.2f, 25.0f, 60.0f, NULL, NULL);
    TEST_ASSERT_FLOAT_WITHIN(0.5f, 12.74f, v);
}

// =============================================================================
// Enrichissement 2026-06 : valider_params_programme, calcul_esp_nominal_m et
// interpolation IDW hors points exacts n'étaient PAS testés (couverture 53 %).
// =============================================================================

// 14 — buse=0 -> calcul impossible (return 0)
static void test_buse_zero_erreur(void)
{
    float debit = 99.0f;
    float v = lookup_vitesse_cible(&ABAQUE_SR150C, 4.9f, 0.0f, 25.0f, 60.0f, &debit, NULL);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.0f, v);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.0f, debit);
}

// 15 — dose<1mm -> guard mathématique strict (return 0, la division divergerait)
static void test_dose_sous_1mm_erreur(void)
{
    float v = lookup_vitesse_cible(&ABAQUE_SR150C, 4.9f, 17.3f, 0.5f, 60.0f, NULL, NULL);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.0f, v);
}

// 16 — interpolation IDW SR150C non-équidistante : p=5.2, buse=17.3
//      Voisins : (4.9, p_buse=3.5) d=0.065 et (5.6, p_buse=4.0) d=0.087
//      p_buse = (3.5/0.065 + 4.0/0.087) / (1/0.065 + 1/0.087) ≈ 3.71
static void test_interp_idw_non_equidistante(void)
{
    float p_buse = 0.0f;
    float v = lookup_vitesse_cible(&ABAQUE_SR150C, 5.2f, 17.3f, 25.0f, 60.0f, NULL, &p_buse);
    TEST_ASSERT_TRUE_MESSAGE(p_buse > 3.5f && p_buse < 4.0f,
                             "p_buse interpole doit etre entre les 2 voisins");
    TEST_ASSERT_FLOAT_WITHIN(0.1f, 3.71f, p_buse);   // plus proche du voisin le plus pres
    TEST_ASSERT_TRUE(v > 0.0f);
}

// 17 — p_buse_out sur hit exact : retourne la valeur table sans interpolation
static void test_p_buse_out_hit_exact(void)
{
    float p_buse = 0.0f;
    lookup_vitesse_cible(&ABAQUE_SR150C, 4.9f, 17.3f, 25.0f, 60.0f, NULL, &p_buse);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 3.5f, p_buse);
}

// 18 — calcul_esp_nominal_m : valeur plausible, croît avec la buse, garde-fous
static void test_esp_nominal(void)
{
    // portee = 7.06 * 17.3^0.557 * (3.5/3.5)^0.30 ≈ 34.5 m ; esp = ×1.55 ≈ 53.5 m
    float esp = calcul_esp_nominal_m(&ABAQUE_SR150C, 4.9f, 17.3f);
    TEST_ASSERT_FLOAT_WITHIN(2.0f, 53.5f, esp);

    // Buse plus grosse → portée plus grande → espacement plus grand
    float esp_grosse = calcul_esp_nominal_m(&ABAQUE_SR150C, 6.8f, 22.9f);
    TEST_ASSERT_TRUE(esp_grosse > esp);

    // Interpolation (hors point exact) : esp reste plausible
    float esp_interp = calcul_esp_nominal_m(&ABAQUE_SR150C, 5.2f, 17.3f);
    TEST_ASSERT_TRUE(esp_interp > 30.0f && esp_interp < 100.0f);

    TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.0f, calcul_esp_nominal_m(NULL, 4.9f, 17.3f));
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.0f, calcul_esp_nominal_m(&ABAQUE_SR150C, 4.9f, 0.0f));
}

// 19 — valider_params_programme : programme nominal → aucun warning
static void test_valider_programme_nominal(void)
{
    float esp = calcul_esp_nominal_m(&ABAQUE_SR150C, 4.9f, 17.3f);
    hydro_warnings_t w = valider_params_programme(&ABAQUE_SR150C, 4.9f, 17.3f, 25.0f, esp);
    TEST_ASSERT_FALSE(w.pression_basse);
    TEST_ASSERT_FALSE(w.pression_haute);
    TEST_ASSERT_FALSE(w.buse_hors_plage);
    TEST_ASSERT_FALSE(w.dose_hors_plage);
    TEST_ASSERT_FALSE(w.esp_pos_chevauchement);
    TEST_ASSERT_FALSE(w.esp_pos_insuf);
}

// 20 — warnings pression : bornes SR150C [4.9, 9.5] × marges 0.75 / 1.25
static void test_valider_warnings_pression(void)
{
    // 3.0 < 4.9×0.75=3.675 → basse
    hydro_warnings_t w = valider_params_programme(&ABAQUE_SR150C, 3.0f, 17.3f, 25.0f, 50.0f);
    TEST_ASSERT_TRUE(w.pression_basse);
    TEST_ASSERT_FALSE(w.pression_haute);

    // 12.5 > 9.5×1.25=11.875 → haute
    w = valider_params_programme(&ABAQUE_SR150C, 12.5f, 17.3f, 25.0f, 50.0f);
    TEST_ASSERT_TRUE(w.pression_haute);
    TEST_ASSERT_FALSE(w.pression_basse);

    // 4.0 : sous p_min mais dans la marge ×0.75 → pas de warning
    w = valider_params_programme(&ABAQUE_SR150C, 4.0f, 17.3f, 25.0f, 50.0f);
    TEST_ASSERT_FALSE(w.pression_basse);
}

// 21 — warnings buse et dose : bornes buse [17.3, 25.4], dose [15, 40] × marges
static void test_valider_warnings_buse_dose(void)
{
    // buse 12 < 17.3×0.75=12.975 → hors plage ; buse 33 > 25.4×1.25=31.75 → hors plage
    hydro_warnings_t w = valider_params_programme(&ABAQUE_SR150C, 6.0f, 12.0f, 25.0f, 50.0f);
    TEST_ASSERT_TRUE(w.buse_hors_plage);
    w = valider_params_programme(&ABAQUE_SR150C, 6.0f, 33.0f, 25.0f, 50.0f);
    TEST_ASSERT_TRUE(w.buse_hors_plage);

    // dose 10 < 15×0.75=11.25 → hors plage ; dose 55 > 40×1.25=50 → hors plage
    w = valider_params_programme(&ABAQUE_SR150C, 6.0f, 17.3f, 10.0f, 50.0f);
    TEST_ASSERT_TRUE(w.dose_hors_plage);
    w = valider_params_programme(&ABAQUE_SR150C, 6.0f, 17.3f, 55.0f, 50.0f);
    TEST_ASSERT_TRUE(w.dose_hors_plage);

    // dose 12 : hors [15,40] mais dans les marges → pas de warning
    w = valider_params_programme(&ABAQUE_SR150C, 6.0f, 17.3f, 12.0f, 50.0f);
    TEST_ASSERT_FALSE(w.dose_hors_plage);
}

// 22 — warnings espacement vs portée calculée (seuils ×0.75 / ×1.10)
static void test_valider_warnings_espacement(void)
{
    float esp = calcul_esp_nominal_m(&ABAQUE_SR150C, 4.9f, 17.3f);

    // largeur = esp/2 → fort recroisement
    hydro_warnings_t w = valider_params_programme(&ABAQUE_SR150C, 4.9f, 17.3f, 25.0f, esp * 0.5f);
    TEST_ASSERT_TRUE(w.esp_pos_chevauchement);
    TEST_ASSERT_FALSE(w.esp_pos_insuf);

    // largeur = esp×1.5 → risque sous-arrosage
    w = valider_params_programme(&ABAQUE_SR150C, 4.9f, 17.3f, 25.0f, esp * 1.5f);
    TEST_ASSERT_TRUE(w.esp_pos_insuf);
    TEST_ASSERT_FALSE(w.esp_pos_chevauchement);

    // largeur non renseignée (0) → warnings espacement neutralisés
    w = valider_params_programme(&ABAQUE_SR150C, 4.9f, 17.3f, 25.0f, 0.0f);
    TEST_ASSERT_FALSE(w.esp_pos_chevauchement);
    TEST_ASSERT_FALSE(w.esp_pos_insuf);
}

// 23 — valider_params_programme : abaque NULL → tout false, pas de crash
static void test_valider_abaque_null(void)
{
    hydro_warnings_t w = valider_params_programme(NULL, 6.0f, 17.3f, 25.0f, 50.0f);
    TEST_ASSERT_FALSE(w.pression_basse || w.pression_haute || w.buse_hors_plage
                      || w.dose_hors_plage || w.esp_pos_chevauchement || w.esp_pos_insuf);
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
    RUN_TEST(test_sr100c_exact_buse127_d25);
    RUN_TEST(test_sr100c_exact_buse190_d30);
    RUN_TEST(test_sr100c_interp_buse152);
    RUN_TEST(test_buse_zero_erreur);
    RUN_TEST(test_dose_sous_1mm_erreur);
    RUN_TEST(test_interp_idw_non_equidistante);
    RUN_TEST(test_p_buse_out_hit_exact);
    RUN_TEST(test_esp_nominal);
    RUN_TEST(test_valider_programme_nominal);
    RUN_TEST(test_valider_warnings_pression);
    RUN_TEST(test_valider_warnings_buse_dose);
    RUN_TEST(test_valider_warnings_espacement);
    RUN_TEST(test_valider_abaque_null);
}
