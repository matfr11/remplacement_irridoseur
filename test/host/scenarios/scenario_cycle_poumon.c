#include "unity.h"
#include "unity_suite.h"
#include "state_machine.h"
#include "gpio_handler.h"
#include "gpio_config.h"
#include "mock_gpio.h"
#include "mock_nvs.h"
#include "mock_timer.h"
#include "mock_log.h"
#include "config_nvs.h"
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

    // Config non-dégradée avec t_vidange court pour des tests rapides
    config_machine_t m = CFG_MACHINE_DEFAUT;
    m.machine_active  = 0;
    m.t_vidange_s     = 2.0f;
    m.mode_deg_poumon = false;
    m.kp_regulation   = 0.1f;
    m.n_cycles_calib  = 3;
    config_nvs_sauver_machine(&m);

    config_programme_t p = {
        .nom = "CycleTest",
        .dose_mm = 20.0f, .largeur_m = 18.0f,
        .buse_mm = 25, .pression_bar = 3.5f,
    };
    config_nvs_sauver_programme(0, &p);
    config_nvs_sauver_prog_actif(0);

    state_machine_init();
}

static void local_tearDown(void) { state_machine_test_reset(); }

// Test — cycle poumon normal via capteur poumon_plein :
//   SOUS_VIDANGE → SOUS_ATTENTE → SOUS_REMPLISSAGE (EV_POUMON=ON)
//   → poumon_plein HIGH → EV_POUMON=OFF, longueur session accumulée
static void test_cycle_normal_poumon_plein(void)
{
    state_machine_test_injecter_etat(ETAT_EN_COURS);
    state_machine_test_set_pression(true);
    gpio_ev_canon_set(true);  // canon ouvert, session active

    // Initialement en SOUS_VIDANGE : poumon fermé
    TEST_ASSERT_FALSE(gpio_ev_poumon_get());

    // t_vidange_s=2s → 20 ticks + 2 ticks (SOUS_ATTENTE→SOUS_REMPLISSAGE)
    // Résultat : SOUS_REMPLISSAGE, EV_POUMON ouvert
    avancer(22);
    TEST_ASSERT_TRUE(gpio_ev_poumon_get());
    TEST_ASSERT_EQUAL_INT(ETAT_EN_COURS, state_machine_get_etat());

    // Capteur poumon_plein déclenché (bobine pleine)
    gpio_set_level(PIN_POUMON_PLEIN, 1);
    avancer(1);

    // Cycle terminé : poumon refermé, cliquet avancé, session progressée
    TEST_ASSERT_FALSE(gpio_ev_poumon_get());
    TEST_ASSERT_EQUAL_INT(ETAT_EN_COURS, state_machine_get_etat());

    machine_status_t s;
    state_machine_get_status(&s);
    TEST_ASSERT_TRUE(s.longueur_enroulee_m > 0.0f);

    state_machine_cmd_stop();
}

void suite_scenario_cycle_poumon(void)
{
    unity_suite_setup(local_setUp, local_tearDown);
    RUN_TEST(test_cycle_normal_poumon_plein);
}
