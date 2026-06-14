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

static void config_set_mode_degrade(bool poumon, float t_rempl)
{
    config_machine_t m = CFG_MACHINE_DEFAUT;
    m.machine_active   = 0;
    m.t_vidange_s      = 2.0f;  // 2s vidange pour tests plus courts
    m.mode_deg_poumon  = poumon;
    m.t_rempl_fixe_s   = t_rempl;
    m.dist_cycle_nvs   = 0.5f;
    m.cycles_par_tour  = 40.0f; // requis pour calcul distance par cycle
    config_nvs_sauver_machine(&m);

    config_programme_t p = {
        .nom          = "ModeTest",
        .dose_mm      = 20.0f,
        .largeur_m    = 18.0f,
        .buse_mm      = 25,
        .pression_bar = 3.5f,
    };
    config_nvs_sauver_programme(0, &p);
    config_nvs_sauver_prog_actif(0);
}

static void local_setUp(void)
{
    mock_nvs_reset();
    mock_time_reset();
    mock_gpio_reset();
    mock_log_reset();
    gpio_handler_test_reset();
}

static void local_tearDown(void) { state_machine_test_reset(); }

// Scénario 12 — mode dégradé B : remplissage temporisé (t_rempl_fixe_s=2s)
static void test_scenario_mode_degrade_b(void)
{
    config_set_mode_degrade(true, 2.0f);
    state_machine_init();

    // Aller en EN_COURS via injection
    state_machine_test_injecter_etat(ETAT_EN_COURS);

    // Après vidange (2s = 20 ticks) → SOUS_ATTENTE → SOUS_REMPLISSAGE
    avancer(22);

    // Mode B : t_rempl_fixe_s=2s → après 20 ticks supplémentaires, fin automatique
    // (sans nécessiter gpio poumon_plein)
    avancer(22);

    // Doit être revenu en SOUS_VIDANGE (1 cycle accompli en mode B)
    machine_status_t st;
    state_machine_get_status(&st);
    TEST_ASSERT_TRUE(st.mode_degrade_poumon);
    TEST_ASSERT_EQUAL_INT(ETAT_EN_COURS, state_machine_get_etat());

    state_machine_cmd_stop();
}

// Scénario 13 — mode dégradé B : remplissage initial (ETAT_REMPLISSAGE_POUMON) via timer
static void test_scenario_mode_degrade_b_initial(void)
{
    config_set_mode_degrade(true, 2.0f);
    state_machine_init();

    // VEILLE → OUVERTURE_CANON (1 tick)
    tick_state_machine();
    mock_time_advance_ms(100);
    TEST_ASSERT_EQUAL_INT(ETAT_OUVERTURE_CANON, state_machine_get_etat());

    // Timer t_ouv_canon_s (20s) écoulé → REMPLISSAGE_POUMON
    mock_time_advance_ms(20000);
    tick_state_machine();
    mock_time_advance_ms(100);
    TEST_ASSERT_EQUAL_INT(ETAT_REMPLISSAGE_POUMON, state_machine_get_etat());

    // 20 ticks (2s) sans gpio poumon_plein → doit transitionner vers EN_COURS
    for (int i = 0; i < 21; i++) {
        tick_state_machine();
        mock_time_advance_ms(100);
    }
    TEST_ASSERT_EQUAL_INT(ETAT_EN_COURS, state_machine_get_etat());

    state_machine_cmd_stop();
}

void suite_scenario_modes_degrades(void)
{
    unity_suite_setup(local_setUp, local_tearDown);
    RUN_TEST(test_scenario_mode_degrade_b);
    RUN_TEST(test_scenario_mode_degrade_b_initial);
}
