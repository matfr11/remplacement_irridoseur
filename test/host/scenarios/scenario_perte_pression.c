#include "unity.h"
#include "state_machine.h"
#include "gpio_config.h"
#include "mock_gpio.h"
#include "mock_nvs.h"
#include "mock_timer.h"
#include "mock_log.h"
#include "test_helpers.h"

// Helpers locaux : tick_state_machine + sim override + GPIO cohérents
static void set_pression(bool ok)
{
    state_machine_test_set_pression(ok);
    // PIN_PRESSOSTAT NC : LOW = pression ok, HIGH = pression absente
    gpio_set_level(PIN_PRESSOSTAT, ok ? 0 : 1);
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

// Scénario 3 — perte pression en EN_COURS → PAUSE_PRESSION → reprise → EN_COURS
static void test_scenario_pause_reprise(void)
{
    state_machine_test_injecter_etat(ETAT_EN_COURS);
    set_pression(true);

    // Couper la pression
    set_pression(false);
    avancer(1);
    TEST_ASSERT_EQUAL_INT(ETAT_PAUSE_PRESSION, state_machine_get_etat());

    // Rétablir la pression
    set_pression(true);
    avancer(1);
    TEST_ASSERT_EQUAL_INT(ETAT_EN_COURS, state_machine_get_etat());

    state_machine_cmd_stop();
}

// Scénario 4 — perte pression pendant REMPLISSAGE_POUMON → PAUSE_PRESSION → reprise
static void test_scenario_perte_pendant_remplissage(void)
{
    state_machine_test_injecter_etat(ETAT_REMPLISSAGE_POUMON);
    set_pression(true);

    set_pression(false);
    avancer(1);
    TEST_ASSERT_EQUAL_INT(ETAT_PAUSE_PRESSION, state_machine_get_etat());

    set_pression(true);
    avancer(1);
    TEST_ASSERT_EQUAL_INT(ETAT_EN_COURS, state_machine_get_etat());

    state_machine_cmd_stop();
}

void suite_scenario_perte_pression(void)
{
    RUN_TEST(test_scenario_pause_reprise);
    RUN_TEST(test_scenario_perte_pendant_remplissage);
}
