#include "unity.h"
#include "unity_suite.h"
#include "state_machine.h"
#include "gpio_handler.h"
#include "gpio_config.h"
#include "mock_gpio.h"
#include "mock_nvs.h"
#include "mock_timer.h"
#include "mock_log.h"
#include "test_helpers.h"

static void local_setUp(void)
{
    mock_nvs_reset();
    mock_time_reset();
    mock_gpio_reset();
    mock_log_reset();
    gpio_handler_test_reset();
    config_set_programme_valide();
    state_machine_init();
}

static void local_tearDown(void) { state_machine_test_reset(); }

// Avance jusqu'à REMPLISSAGE_POUMON (via OUVERTURE_CANON, 32 ticks max)
static int avancer_vers_remplissage(void)
{
    // 1 tick → OUVERTURE_CANON
    tick_state_machine();
    mock_time_advance_ms(100);
    if (state_machine_get_etat() != ETAT_OUVERTURE_CANON) return -1;

    // 30 ticks (pression stable) → REMPLISSAGE_POUMON
    for (int i = 0; i < 31; i++) {
        tick_state_machine();
        mock_time_advance_ms(100);
    }
    return (state_machine_get_etat() == ETAT_REMPLISSAGE_POUMON) ? 0 : -2;
}

// Scénario 1 — cycle complet sans tempo
static void test_scenario_cycle_sans_tempo(void)
{
    TEST_ASSERT_EQUAL_INT(0, avancer_vers_remplissage());

    // Poumon plein → EN_COURS
    gpio_set_level(PIN_POUMON_PLEIN, 1);
    tick_state_machine();
    mock_time_advance_ms(100);
    TEST_ASSERT_EQUAL_INT(ETAT_EN_COURS, state_machine_get_etat());

    // Reset poumon (pas en permanence plein)
    gpio_set_level(PIN_POUMON_PLEIN, 0);

    // Avancer en SOUS_VIDANGE jusqu'à SOUS_REMPLISSAGE (t_vidange=5s → 50 ticks)
    for (int i = 0; i < 55; i++) {
        tick_state_machine();
        mock_time_advance_ms(100);
    }
    // Maintenant en SOUS_REMPLISSAGE — simuler poumon plein pour terminer le cycle
    gpio_set_level(PIN_POUMON_PLEIN, 1);
    tick_state_machine();
    mock_time_advance_ms(100);
    gpio_set_level(PIN_POUMON_PLEIN, 0);

    // Doit être revenu en SOUS_VIDANGE (1 cycle effectué)
    machine_status_t st;
    state_machine_get_status(&st);
    TEST_ASSERT_EQUAL_INT(ETAT_EN_COURS, state_machine_get_etat());

    // cmd_stop → ARRET_FINAL
    state_machine_cmd_stop();
    TEST_ASSERT_EQUAL_INT(ETAT_ARRET_FINAL, state_machine_get_etat());
}

// Scénario 2 — cycle avec tempo départ
static void test_scenario_cycle_avec_tempo(void)
{
    // Sauvegarder un programme avec tempo_depart_on=true, 2s
    config_programme_t p = {
        .nom              = "TempoTest",
        .dose_mm          = 20.0f,
        .largeur_m        = 18.0f,
        .buse_mm          = 25,
        .pression_bar     = 3.5f,
        .tempo_depart_on  = true,
        .tempo_depart_s   = 2,
        .tempo_arrivee_on = false,
        .tempo_arrivee_s  = 0,
    };
    config_nvs_sauver_programme(0, &p);
    state_machine_init();

    // VEILLE → OUVERTURE_CANON (mise en pression par l'opérateur)
    tick_state_machine();
    mock_time_advance_ms(100);
    TEST_ASSERT_EQUAL_INT(ETAT_OUVERTURE_CANON, state_machine_get_etat());

    // Pression stable 30 ticks → TEMPO_DEPART
    for (int i = 0; i < 31; i++) {
        tick_state_machine();
        mock_time_advance_ms(100);
    }
    TEST_ASSERT_EQUAL_INT(ETAT_TEMPO_DEPART, state_machine_get_etat());

    // 20 ticks (2s tempo) → REMPLISSAGE_POUMON
    for (int i = 0; i < 21; i++) {
        tick_state_machine();
        mock_time_advance_ms(100);
    }
    TEST_ASSERT_EQUAL_INT(ETAT_REMPLISSAGE_POUMON, state_machine_get_etat());

    // Poumon plein → EN_COURS
    gpio_set_level(PIN_POUMON_PLEIN, 1);
    tick_state_machine();
    mock_time_advance_ms(100);
    TEST_ASSERT_EQUAL_INT(ETAT_EN_COURS, state_machine_get_etat());
    gpio_set_level(PIN_POUMON_PLEIN, 0);

    // Arrêt propre
    state_machine_cmd_stop();
    TEST_ASSERT_EQUAL_INT(ETAT_ARRET_FINAL, state_machine_get_etat());
}

void suite_scenario_cycle_normal(void)
{
    unity_suite_setup(local_setUp, local_tearDown);
    RUN_TEST(test_scenario_cycle_sans_tempo);
    RUN_TEST(test_scenario_cycle_avec_tempo);
}
