#include "unity.h"
#include "unity_suite.h"
#include "state_machine.h"
#include "gpio_handler.h"
#include "mock_nvs.h"
#include "mock_timer.h"
#include "mock_gpio.h"
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

// 1 — état initial = VEILLE
static void test_etat_initial(void)
{
    TEST_ASSERT_EQUAL_INT(ETAT_VEILLE, state_machine_get_etat());
}

// 2 — urgence depuis VEILLE → ARRET_URGENCE
static void test_urgence_depuis_veille(void)
{
    state_machine_declencher_urgence("test");
    TEST_ASSERT_EQUAL_INT(ETAT_ARRET_URGENCE, state_machine_get_etat());
}

// 3 — cmd_reset après urgence → VEILLE
static void test_reset_apres_urgence(void)
{
    state_machine_declencher_urgence("test");
    state_machine_cmd_reset();
    TEST_ASSERT_EQUAL_INT(ETAT_VEILLE, state_machine_get_etat());
}

// 4 — urgence depuis EN_COURS → ARRET_URGENCE
static void test_urgence_depuis_en_cours(void)
{
    state_machine_test_injecter_etat(ETAT_EN_COURS);
    state_machine_declencher_urgence("SEC-2");
    TEST_ASSERT_EQUAL_INT(ETAT_ARRET_URGENCE, state_machine_get_etat());
    state_machine_cmd_reset();
}

// 5 — cmd_stop depuis EN_COURS → ARRET_FINAL
static void test_stop_depuis_en_cours(void)
{
    state_machine_test_injecter_etat(ETAT_EN_COURS);
    state_machine_cmd_stop();
    TEST_ASSERT_EQUAL_INT(ETAT_ARRET_FINAL, state_machine_get_etat());
}

// 6 — cmd_stop depuis ARRET_URGENCE → noop
static void test_stop_noop_urgence(void)
{
    state_machine_test_injecter_etat(ETAT_ARRET_URGENCE);
    state_machine_cmd_stop();
    TEST_ASSERT_EQUAL_INT(ETAT_ARRET_URGENCE, state_machine_get_etat());
    state_machine_cmd_reset();
}

// 7 — test_reset() : état revient à VEILLE, nb_tentatives = 0
static void test_hook_reset(void)
{
    state_machine_test_injecter_etat(ETAT_EN_COURS);
    state_machine_test_reset();
    TEST_ASSERT_EQUAL_INT(ETAT_VEILLE, state_machine_get_etat());
    TEST_ASSERT_EQUAL_INT(0, state_machine_test_get_nb_tentatives());
}

// 8 — pression perdue → PAUSE_PRESSION (en injectant l'état + 1 tick sans pression)
static void test_pause_pression(void)
{
    state_machine_test_injecter_etat(ETAT_EN_COURS);
    state_machine_test_set_pression(false);
    tick_state_machine();
    mock_time_advance_ms(100);
    TEST_ASSERT_EQUAL_INT(ETAT_PAUSE_PRESSION, state_machine_get_etat());
    state_machine_test_reset();
}

// 9 — pression revenue → reprise EN_COURS depuis PAUSE_PRESSION (1 tick)
static void test_reprise_depuis_pause(void)
{
    state_machine_test_injecter_etat(ETAT_PAUSE_PRESSION);
    state_machine_test_set_pression(false);
    tick_state_machine();
    mock_time_advance_ms(100);
    state_machine_test_set_pression(true);
    tick_state_machine();
    mock_time_advance_ms(100);
    TEST_ASSERT_EQUAL_INT(ETAT_EN_COURS, state_machine_get_etat());
    state_machine_test_reset();
}

// 10 — SEC-2 (secu_spires) → ARRET_URGENCE depuis n'importe quel état
static void test_sec2_priorite(void)
{
    state_machine_test_injecter_etat(ETAT_EN_COURS);
    state_machine_test_set_secu_spires(true);
    tick_state_machine();
    mock_time_advance_ms(100);
    TEST_ASSERT_EQUAL_INT(ETAT_ARRET_URGENCE, state_machine_get_etat());
    state_machine_cmd_reset();
}

// 11 — fin_course → SEC-1 : OUVERTURE_CANON → ARRET_URGENCE
static void test_sec1_ouverture_canon(void)
{
    state_machine_test_injecter_etat(ETAT_OUVERTURE_CANON);
    state_machine_test_set_fin_course(true);
    state_machine_test_set_pression(true);
    tick_state_machine();
    mock_time_advance_ms(100);
    TEST_ASSERT_EQUAL_INT(ETAT_ARRET_URGENCE, state_machine_get_etat());
    state_machine_cmd_reset();
}

// 12 — fin_course ignoré pendant TEMPO_ARRIVEE (c'est normal)
static void test_fin_course_tempo_arrivee(void)
{
    state_machine_test_injecter_etat(ETAT_TEMPO_ARRIVEE);
    state_machine_test_set_fin_course(true);
    state_machine_test_set_pression(true);
    tick_state_machine();
    mock_time_advance_ms(100);
    // Doit rester TEMPO_ARRIVEE ou transitionner normalement (pas ARRET_URGENCE)
    etat_machine_t e = state_machine_get_etat();
    TEST_ASSERT_NOT_EQUAL(ETAT_ARRET_URGENCE, e);
    state_machine_test_reset();
}

// 13 — reload config en VEILLE → reste VEILLE
static void test_reload_veille(void)
{
    state_machine_recharger_config();
    TEST_ASSERT_EQUAL_INT(ETAT_VEILLE, state_machine_get_etat());
}

// 14 — reload config hors VEILLE → ignoré
static void test_reload_hors_veille(void)
{
    state_machine_test_injecter_etat(ETAT_EN_COURS);
    state_machine_recharger_config();
    TEST_ASSERT_EQUAL_INT(ETAT_EN_COURS, state_machine_get_etat());
    state_machine_test_reset();
}

// 15 — get_nb_tentatives : 0 à l'init, incrémenté par test_hook_reset test
static void test_get_nb_tentatives_init(void)
{
    TEST_ASSERT_EQUAL_INT(0, state_machine_test_get_nb_tentatives());
}

void suite_state_machine(void)
{
    unity_suite_setup(local_setUp, local_tearDown);
    RUN_TEST(test_etat_initial);
    RUN_TEST(test_urgence_depuis_veille);
    RUN_TEST(test_reset_apres_urgence);
    RUN_TEST(test_urgence_depuis_en_cours);
    RUN_TEST(test_stop_depuis_en_cours);
    RUN_TEST(test_stop_noop_urgence);
    RUN_TEST(test_hook_reset);
    RUN_TEST(test_pause_pression);
    RUN_TEST(test_reprise_depuis_pause);
    RUN_TEST(test_sec2_priorite);
    RUN_TEST(test_sec1_ouverture_canon);
    RUN_TEST(test_fin_course_tempo_arrivee);
    RUN_TEST(test_reload_veille);
    RUN_TEST(test_reload_hors_veille);
    RUN_TEST(test_get_nb_tentatives_init);
}
