#include "unity.h"
#include "unity_suite.h"
#include "state_machine.h"
#include "gpio_handler.h"
#include "gpio_config.h"
#include "mock_gpio.h"
#include "mock_nvs.h"
#include "mock_timer.h"
#include "mock_log.h"
#include "mock_ina3221.h"
#include "test_helpers.h"
#include "batterie.h"

// CH3 = batterie (INA3221_CH_BATTERIE)
#define CH_BATT 3

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
    mock_ina3221_reset();
    gpio_handler_test_reset();
    config_set_programme_valide();
    state_machine_init();
    // s_batt_tick=299 garanti par state_machine_test_reset() en tearDown
    // → premier tick du test lit toujours la batterie
}

static void local_tearDown(void) { state_machine_test_reset(); }

// Scénario 17 — Batterie faible (< seuil warn = 11.5V)
//   Cycle continue, alerte visible dans le statut, pas d'arrêt
static void test_scenario_batterie_faible(void)
{
    mock_ina3221_set_canal(CH_BATT, 11.4f, 0);
    state_machine_test_injecter_etat(ETAT_EN_COURS);

    // 1 tick → lecture batterie (s_batt_tick=299 → +1 = 300 >= 300)
    avancer(1);

    TEST_ASSERT_EQUAL_INT(ETAT_EN_COURS, state_machine_get_etat());

    machine_status_t s;
    state_machine_get_status(&s);
    TEST_ASSERT_EQUAL_INT(BATT_ETAT_FAIBLE, s.batterie_etat);
    TEST_ASSERT_FLOAT_WITHIN(0.05f, 11.4f, s.batterie_v);

    // Poll accéléré à 5s : après 50 ticks, une nouvelle lecture a eu lieu
    // On vérifie en passant en critique et en attendant <= 50 ticks
    mock_ina3221_set_canal(CH_BATT, 10.9f, 0);
    for (int i = 0; i < 51; i++) {
        if (state_machine_get_etat() == ETAT_ARRET_URGENCE) break;
        tick_state_machine();
        mock_time_advance_ms(100);
    }
    TEST_ASSERT_EQUAL_INT_MESSAGE(ETAT_ARRET_URGENCE, state_machine_get_etat(),
        "urgence doit se declencher en <= 50 ticks (poll 5s) apres passage en critique");
}

// Scénario 18 — Batterie critique (< seuil crit = 11.0V) en session active
//   → ARRET_URGENCE immédiat, EVs fermées, raison persistée
static void test_scenario_batterie_critique(void)
{
    mock_ina3221_set_canal(CH_BATT, 10.9f, 0);
    state_machine_test_injecter_etat(ETAT_EN_COURS);

    avancer(1);  // lecture batterie au 1er tick → BATT_ETAT_CRITIQUE → urgence

    TEST_ASSERT_EQUAL_INT(ETAT_ARRET_URGENCE, state_machine_get_etat());
    ASSERT_EVS(false, false);  // EVs fermées par declencher_urgence()

    machine_status_t s;
    state_machine_get_status(&s);
    TEST_ASSERT_NOT_NULL(strstr(s.raison_arret, "Batterie critique"));
    TEST_ASSERT_EQUAL_INT(BATT_ETAT_CRITIQUE, s.batterie_etat);
}

// Scénario 18b — Batterie critique hors session (VEILLE, EVs déjà fermées)
//   → pas d'urgence déclenchée (état non actif)
static void test_scenario_batterie_critique_veille(void)
{
    mock_ina3221_set_canal(CH_BATT, 10.9f, 0);
    // Forcer VEILLE avec pression absente pour éviter l'auto-démarrage
    state_machine_test_injecter_etat(ETAT_VEILLE);
    state_machine_test_set_pression(false);

    avancer(1);  // lecture batterie → BATT_ETAT_CRITIQUE, VEILLE ignorée par le switch

    TEST_ASSERT_EQUAL_INT(ETAT_VEILLE, state_machine_get_etat());
    ASSERT_EVS(false, false);
}

void suite_scenario_batterie(void)
{
    unity_suite_setup(local_setUp, local_tearDown);
    RUN_TEST(test_scenario_batterie_faible);
    RUN_TEST(test_scenario_batterie_critique);
    RUN_TEST(test_scenario_batterie_critique_veille);
}
