#include "unity.h"

void unity_suite_setup(void (*su)(void), void (*td)(void));
#include "mosfet_surveillance.h"
#include "state_machine.h"
#include "gpio_config.h"
#include "mock_ina3221.h"
#include "mock_gpio.h"

// =============================================================================
// test_mosfet_surveillance.c — Module réel (était un mock vide avant 2026-06)
// Couvre les bugs #2/#3 de la revue + la logique PR-19 (basculement secours).
//
// Convention sim : mosfet_sim_set_mesure(pin, tension, courant) injecte la
// mesure INA3221 du canal — tension > 6V = EV alimentée, courant > 50mA = OK.
// =============================================================================

static void reset_mosfet(void)
{
    mosfet_test_reset();
    state_machine_test_reset();
    mock_ina3221_reset();
    mock_gpio_reset();
    mosfet_sim_enable(true);
    // Mesures cohérentes "tout éteint" : GPIO=LOW, 0V, 0mA
    mosfet_sim_set_mesure(PIN_EV_CANON,  0.0f, 0.0f);
    mosfet_sim_set_mesure(PIN_EV_POUMON, 0.0f, 0.0f);
}

// ── États sains : aucune bascule ─────────────────────────────────────────────

static void test_mosfet_coherent_off_pas_de_bascule(void)
{
    reset_mosfet();
    mosfet_verifier_apres(PIN_EV_CANON, false);   // GPIO=LOW, 0V → cohérent
    TEST_ASSERT_FALSE(mosfet_secours_actif(PIN_EV_CANON));
    TEST_ASSERT_EQUAL_INT(MOSFET_OK, mosfet_get_etat(PIN_EV_CANON).etat_principal);
}

static void test_mosfet_coherent_on_pas_de_bascule(void)
{
    reset_mosfet();
    mosfet_sim_set_mesure(PIN_EV_CANON, 12.0f, 800.0f);  // alimentée + courant
    mosfet_verifier_apres(PIN_EV_CANON, true);
    TEST_ASSERT_FALSE(mosfet_secours_actif(PIN_EV_CANON));
}

// ── Détection des 3 pannes ───────────────────────────────────────────────────

static void test_mosfet_grille_cc_bascule(void)
{
    reset_mosfet();
    // GPIO=LOW mais tension présente → MOSFET grillé en court-circuit
    mosfet_sim_set_mesure(PIN_EV_CANON, 12.0f, 800.0f);
    mosfet_verifier_avant(PIN_EV_CANON, false);

    TEST_ASSERT_TRUE(mosfet_secours_actif(PIN_EV_CANON));
    TEST_ASSERT_EQUAL_INT(MOSFET_GRILLE_CC, mosfet_get_etat(PIN_EV_CANON).etat_principal);
    // Le relais du canal a commuté (COM → NO)
    TEST_ASSERT_EQUAL_INT(1, gpio_get_level(PIN_RELAIS_CANON));
    // L'autre canal n'est pas affecté
    TEST_ASSERT_FALSE(mosfet_secours_actif(PIN_EV_POUMON));
    TEST_ASSERT_EQUAL_INT(0, gpio_get_level(PIN_RELAIS_POUMON));
}

static void test_mosfet_hs_ouvert_bascule(void)
{
    reset_mosfet();
    // GPIO=HIGH mais 0V → MOSFET HS circuit ouvert
    mosfet_sim_set_mesure(PIN_EV_POUMON, 0.0f, 0.0f);
    mosfet_verifier_apres(PIN_EV_POUMON, true);

    TEST_ASSERT_TRUE(mosfet_secours_actif(PIN_EV_POUMON));
    TEST_ASSERT_EQUAL_INT(MOSFET_HS_OUVERT, mosfet_get_etat(PIN_EV_POUMON).etat_principal);
    TEST_ASSERT_EQUAL_INT(1, gpio_get_level(PIN_RELAIS_POUMON));
}

static void test_mosfet_ev_debranchee_bascule(void)
{
    reset_mosfet();
    // GPIO=HIGH, tension OK mais courant nul → EV débranchée / fil coupé
    mosfet_sim_set_mesure(PIN_EV_CANON, 12.0f, 5.0f);
    mosfet_verifier_apres(PIN_EV_CANON, true);

    TEST_ASSERT_TRUE(mosfet_secours_actif(PIN_EV_CANON));
    TEST_ASSERT_EQUAL_INT(MOSFET_EV_DEBRANCHEE, mosfet_get_etat(PIN_EV_CANON).etat_principal);
}

// ── Basculement : OUT3/OUT4 synchronisé avec l'état du principal ─────────────

static void test_mosfet_bascule_synchronise_pin_secours(void)
{
    reset_mosfet();
    // EV ouverte (pin principal HIGH) au moment de la panne
    gpio_set_level(PIN_EV_CANON, 1);
    mosfet_sim_set_mesure(PIN_EV_CANON, 12.0f, 5.0f);  // EV débranchée
    mosfet_verifier_apres(PIN_EV_CANON, true);

    // OUT3 doit reprendre l'état HIGH du principal (pas de glitch EV)
    TEST_ASSERT_EQUAL_INT(1, gpio_get_level(PIN_MOSFET_SECOURS_CANON));
}

// ── Inhibition sur secours (guard câblage INA post-relais) ───────────────────

static void test_mosfet_pas_de_detection_sur_secours(void)
{
    reset_mosfet();
    // 1re panne → bascule
    mosfet_sim_set_mesure(PIN_EV_CANON, 12.0f, 800.0f);
    mosfet_verifier_avant(PIN_EV_CANON, false);
    TEST_ASSERT_TRUE(mosfet_secours_actif(PIN_EV_CANON));

    // Mesure incohérente sur le secours → délibérément ignorée
    // (anomalie transitoire ≠ double-panne, voir commentaire verifier_coherence)
    mosfet_sim_set_mesure(PIN_EV_CANON, 12.0f, 800.0f);
    mosfet_verifier_apres(PIN_EV_CANON, false);
    TEST_ASSERT_EQUAL_INT(MOSFET_OK, mosfet_get_etat(PIN_EV_CANON).etat_secours);
    // Pas d'urgence déclenchée
    TEST_ASSERT_NOT_EQUAL(ETAT_ARRET_URGENCE, state_machine_get_etat());
}

// ── Reset état ────────────────────────────────────────────────────────────────

static void test_mosfet_reset_etat_remet_principal(void)
{
    reset_mosfet();
    mosfet_sim_set_mesure(PIN_EV_CANON, 12.0f, 800.0f);
    mosfet_verifier_avant(PIN_EV_CANON, false);
    TEST_ASSERT_TRUE(mosfet_secours_actif(PIN_EV_CANON));

    mosfet_reset_etat();
    TEST_ASSERT_FALSE(mosfet_secours_actif(PIN_EV_CANON));
    TEST_ASSERT_EQUAL_INT(MOSFET_OK, mosfet_get_etat(PIN_EV_CANON).etat_principal);
}

// ── Sans INA3221 : surveillance neutralisée ──────────────────────────────────

static void test_mosfet_sans_ina_jamais_de_bascule(void)
{
    reset_mosfet();
    mosfet_sim_enable(false);
    mock_ina3221_set_ok(false);

    mosfet_verifier_avant(PIN_EV_CANON, false);
    mosfet_verifier_apres(PIN_EV_CANON, true);
    mosfet_verifier_post_tick();

    TEST_ASSERT_FALSE(mosfet_secours_actif(PIN_EV_CANON));
    TEST_ASSERT_FALSE(mosfet_secours_actif(PIN_EV_POUMON));
}

// ── Chaînes d'état (utilisées par le JSON statut) ────────────────────────────

static void test_mosfet_etat_str(void)
{
    TEST_ASSERT_EQUAL_STRING("OK",            mosfet_etat_str(MOSFET_OK));
    TEST_ASSERT_EQUAL_STRING("grillé CC",     mosfet_etat_str(MOSFET_GRILLE_CC));
    TEST_ASSERT_EQUAL_STRING("HS ouvert",     mosfet_etat_str(MOSFET_HS_OUVERT));
    TEST_ASSERT_EQUAL_STRING("EV débranchée", mosfet_etat_str(MOSFET_EV_DEBRANCHEE));
}

// ── Point d'entrée ────────────────────────────────────────────────────────────

void suite_mosfet_surveillance(void)
{
    unity_suite_setup(NULL, NULL);
    RUN_TEST(test_mosfet_coherent_off_pas_de_bascule);
    RUN_TEST(test_mosfet_coherent_on_pas_de_bascule);
    RUN_TEST(test_mosfet_grille_cc_bascule);
    RUN_TEST(test_mosfet_hs_ouvert_bascule);
    RUN_TEST(test_mosfet_ev_debranchee_bascule);
    RUN_TEST(test_mosfet_bascule_synchronise_pin_secours);
    RUN_TEST(test_mosfet_pas_de_detection_sur_secours);
    RUN_TEST(test_mosfet_reset_etat_remet_principal);
    RUN_TEST(test_mosfet_sans_ina_jamais_de_bascule);
    RUN_TEST(test_mosfet_etat_str);

    // Nettoyage : ne pas polluer les scénarios suivants
    mosfet_test_reset();
    mosfet_sim_enable(false);
    mock_ina3221_reset();
    state_machine_test_reset();
}
