#include "unity.h"
#include "state_machine.h"
#include "gpio_config.h"
#include "mock_gpio.h"
#include "mock_nvs.h"
#include "mock_timer.h"
#include "mock_log.h"
#include "test_helpers.h"

static void set_fin_course(bool active)
{
    state_machine_test_set_fin_course(active);
    gpio_set_level(PIN_FIN_COURSE, active ? 1 : 0);
}

static void set_secu_spires(bool active)
{
    state_machine_test_set_secu_spires(active);
    gpio_set_level(PIN_SECU_SPIRES, active ? 1 : 0);
}

static void avancer(int n)
{
    for (int i = 0; i < n; i++) {
        tick_state_machine();
        mock_time_advance_ms(100);
    }
}

void setUp(void)
{
    mock_nvs_reset();
    mock_time_reset();
    mock_gpio_reset();
    mock_log_reset();
    gpio_handler_test_reset();
    config_set_programme_valide();
    state_machine_init();
}

void tearDown(void) { state_machine_test_reset(); }

// Scénario 5 — SEC-2 (secu_spires) depuis EN_COURS → ARRET_URGENCE
static void test_scenario_sec2_en_cours(void)
{
    state_machine_test_injecter_etat(ETAT_EN_COURS);
    set_secu_spires(true);
    avancer(1);
    TEST_ASSERT_EQUAL_INT(ETAT_ARRET_URGENCE, state_machine_get_etat());
    state_machine_cmd_reset();
}

// Scénario 6 — SEC-1 (fin_course) depuis OUVERTURE_CANON → ARRET_URGENCE
static void test_scenario_sec1_ouverture_canon(void)
{
    state_machine_test_injecter_etat(ETAT_OUVERTURE_CANON);
    set_fin_course(true);
    avancer(1);
    TEST_ASSERT_EQUAL_INT(ETAT_ARRET_URGENCE, state_machine_get_etat());
    state_machine_cmd_reset();
}

// Scénario 7 — SEC-1 (fin_course) depuis EN_COURS → ARRET_URGENCE
static void test_scenario_sec1_en_cours(void)
{
    state_machine_test_injecter_etat(ETAT_EN_COURS);
    set_fin_course(true);
    avancer(1);
    TEST_ASSERT_EQUAL_INT(ETAT_ARRET_URGENCE, state_machine_get_etat());
    state_machine_cmd_reset();
}

// Scénario 9 — timeout poumon 2 tentatives → ARRET_URGENCE
static void test_scenario_timeout_poumon_2tentatives(void)
{
    state_machine_test_injecter_etat(ETAT_REMPLISSAGE_POUMON);
    // Poumon_plein reste LOW → timeout 20s (200 ticks)
    // Tentative 1 : 200 ticks → timeout, s_nb_tentatives = 1 → réentrée
    avancer(201);
    TEST_ASSERT_EQUAL_INT(1, state_machine_test_get_nb_tentatives());
    // Tentative 2 : 200 ticks supplémentaires → s_nb_tentatives = 2 → urgence
    avancer(201);
    TEST_ASSERT_EQUAL_INT(ETAT_ARRET_URGENCE, state_machine_get_etat());
    state_machine_cmd_reset();
}

// Scénario 10 — urgence depuis PAUSE_PRESSION
static void test_scenario_urgence_depuis_pause(void)
{
    state_machine_test_injecter_etat(ETAT_PAUSE_PRESSION);
    set_secu_spires(true);
    avancer(1);
    TEST_ASSERT_EQUAL_INT(ETAT_ARRET_URGENCE, state_machine_get_etat());
    state_machine_cmd_reset();
}

void suite_scenario_urgences(void)
{
    RUN_TEST(test_scenario_sec2_en_cours);
    RUN_TEST(test_scenario_sec1_ouverture_canon);
    RUN_TEST(test_scenario_sec1_en_cours);
    RUN_TEST(test_scenario_timeout_poumon_2tentatives);
    RUN_TEST(test_scenario_urgence_depuis_pause);
}
