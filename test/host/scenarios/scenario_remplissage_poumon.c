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

static void avancer(int n)
{
    for (int i = 0; i < n; i++) {
        tick_state_machine();
        mock_time_advance_ms(100);
    }
}

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

// Test A — abaque_nom dans le statut après state_machine_init
static void test_abaque_nom_dans_statut(void)
{
    machine_status_t s;
    state_machine_get_status(&s);
    // CFG_MACHINE_DEFAUT : machine_active=0 → abaque_idx=0 → SR 150C
    TEST_ASSERT_EQUAL_STRING("SR 150C", s.abaque_nom);
}

// Test B — retry remplissage poumon : délai t_vidange_s avant ré-ouverture EV_POUMON
static void test_retry_delai_vidange(void)
{
    // t_vidange_s=5.0f (défini dans config_set_programme_valide)
    state_machine_test_injecter_etat(ETAT_REMPLISSAGE_POUMON);
    state_machine_test_set_pression(true);
    gpio_set_level(PIN_PRESSOSTAT, 0);   // pression ok (active LOW)
    gpio_set_level(PIN_POUMON_PLEIN, 0); // poumon pas plein

    // Avancer jusqu'au timeout 20 s → 1ère tentative échoue → nb_tentatives=1
    avancer(201); // 200 ticks × 100 ms = 20 s + 1 tick de transition
    TEST_ASSERT_EQUAL_INT(1, state_machine_test_get_nb_tentatives());

    // EV_POUMON doit être OFF (vidange en cours) immédiatement après le timeout
    TEST_ASSERT_EQUAL_INT(0, gpio_get_level(PIN_EV_POUMON));

    // Dans les 4 s suivantes (< t_vidange_s=5 s) : EV_POUMON reste OFF
    avancer(40); // 40 ticks = 4 s
    TEST_ASSERT_EQUAL_INT(0, gpio_get_level(PIN_EV_POUMON));

    // Après 5 s total de vidange (15 ticks de plus → dépasse t_vidange_s)
    avancer(15); // 1.5 s supplémentaires → total > 5 s
    TEST_ASSERT_EQUAL_INT(1, gpio_get_level(PIN_EV_POUMON)); // ré-ouvert
    TEST_ASSERT_EQUAL_INT(ETAT_REMPLISSAGE_POUMON, state_machine_get_etat());

    state_machine_cmd_stop();
}

void suite_scenario_remplissage_poumon(void)
{
    unity_suite_setup(local_setUp, local_tearDown);
    RUN_TEST(test_abaque_nom_dans_statut);
    RUN_TEST(test_retry_delai_vidange);
}
