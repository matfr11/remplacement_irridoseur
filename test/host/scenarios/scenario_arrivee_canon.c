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
    config_set_programme_valide();
    state_machine_init();
}

static void local_tearDown(void) { state_machine_test_reset(); }

// Test A — arrivée canon sans tempo_arrivee :
//   EN_COURS → fin_course HIGH → EV_POUMON=OFF, EV_CANON=OFF → ARRET_FINAL → VEILLE
static void test_arrivee_sans_tempo(void)
{
    state_machine_test_injecter_etat(ETAT_EN_COURS);
    gpio_ev_canon_set(true);   // canon ouvert pendant l'arrosage
    state_machine_test_set_pression(true);

    // Le canon arrive en bout de course → fin_course passe HIGH
    state_machine_test_set_fin_course(true);
    avancer(1);

    // Sécurité fin de course déclenchée : arrêt immédiat des deux vannes
    TEST_ASSERT_EQUAL_INT(ETAT_ARRET_FINAL, state_machine_get_etat());
    TEST_ASSERT_FALSE(gpio_ev_poumon_get());  // poumon coupé
    TEST_ASSERT_FALSE(gpio_ev_canon_get());   // canon fermé

    // Capteur libéré → retour en VEILLE
    state_machine_test_set_fin_course(false);
    avancer(1);
    TEST_ASSERT_EQUAL_INT(ETAT_VEILLE, state_machine_get_etat());
}

// Test B — arrivée canon avec tempo_arrivee (10 s) :
//   EN_COURS → fin_course HIGH → EV_POUMON=OFF, EV_CANON=ON → TEMPO_ARRIVEE
//   → expiration 10 s → EV_CANON=OFF → ARRET_FINAL
static void test_arrivee_avec_tempo(void)
{
    // Programme avec 10 s d'arrosage sur place à l'arrivée
    config_programme_t prog = {
        .nom              = "Tempo",
        .dose_mm          = 20.0f,
        .largeur_m        = 18.0f,
        .buse_mm          = 25,
        .pression_bar     = 3.5f,
        .tempo_arrivee_on = true,
        .tempo_arrivee_s  = 10,
    };
    config_nvs_sauver_programme(0, &prog);
    state_machine_recharger_config();   // on est en VEILLE après init

    state_machine_test_injecter_etat(ETAT_EN_COURS);
    gpio_ev_canon_set(true);
    state_machine_test_set_pression(true);

    // Le canon arrive → fin_course HIGH → entrée TEMPO_ARRIVEE
    state_machine_test_set_fin_course(true);
    avancer(1);
    TEST_ASSERT_EQUAL_INT(ETAT_TEMPO_ARRIVEE, state_machine_get_etat());
    TEST_ASSERT_FALSE(gpio_ev_poumon_get());  // poumon coupé
    TEST_ASSERT_TRUE(gpio_ev_canon_get());   // canon encore ouvert

    // À 9,9 s : le timer n'a pas encore expiré
    avancer(99);
    TEST_ASSERT_EQUAL_INT(ETAT_TEMPO_ARRIVEE, state_machine_get_etat());
    TEST_ASSERT_TRUE(gpio_ev_canon_get());

    // Au 100e tick (t_dans_etat = 10 s) : expiration → canon fermé
    avancer(1);
    TEST_ASSERT_EQUAL_INT(ETAT_ARRET_FINAL, state_machine_get_etat());
    TEST_ASSERT_FALSE(gpio_ev_canon_get());
}

// Test C — pression coupée pendant TEMPO_ARRIVEE : canon fermé immédiatement
static void test_arrivee_pression_coupee_pendant_tempo(void)
{
    config_programme_t prog = {
        .nom              = "TempoP",
        .dose_mm          = 20.0f,
        .largeur_m        = 18.0f,
        .buse_mm          = 25,
        .pression_bar     = 3.5f,
        .tempo_arrivee_on = true,
        .tempo_arrivee_s  = 30,   // long délai — ne doit pas expirer
    };
    config_nvs_sauver_programme(0, &prog);
    state_machine_recharger_config();

    state_machine_test_injecter_etat(ETAT_EN_COURS);
    gpio_ev_canon_set(true);
    state_machine_test_set_pression(true);

    // Déclenchement fin de course → TEMPO_ARRIVEE
    state_machine_test_set_fin_course(true);
    avancer(1);
    TEST_ASSERT_EQUAL_INT(ETAT_TEMPO_ARRIVEE, state_machine_get_etat());
    TEST_ASSERT_TRUE(gpio_ev_canon_get());

    // Pression perdue en cours d'arrosage sur place → fermeture immédiate du canon
    state_machine_test_set_pression(false);
    avancer(1);
    TEST_ASSERT_EQUAL_INT(ETAT_ARRET_FINAL, state_machine_get_etat());
    TEST_ASSERT_FALSE(gpio_ev_canon_get());
}

void suite_scenario_arrivee_canon(void)
{
    unity_suite_setup(local_setUp, local_tearDown);
    RUN_TEST(test_arrivee_sans_tempo);
    RUN_TEST(test_arrivee_avec_tempo);
    RUN_TEST(test_arrivee_pression_coupee_pendant_tempo);
}
