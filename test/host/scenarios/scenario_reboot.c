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
    state_machine_init();  // premier démarrage propre
}

static void local_tearDown(void) { state_machine_test_reset(); }

// Test A — coupure courant détectée au boot :
//   NVS contient session_active=true + longueurs → s_coupure_detectee=true,
//   longueurs restaurées, machine ne démarre pas seule
static void test_coupure_courant_detectee(void)
{
    // Simule l'état NVS après une session interrompue à 20m sur 100m déroulés
    // (tuyau ST1 Bis 330m : abs_start = 330-100 = 230m, session = 250-230 = 20m)
    config_nvs_sauver_session_active(true);
    config_nvs_sauver_deroule(100.0f);
    config_nvs_sauver_longueur(250.0f);

    // Redémarrage (simule la mise sous tension après coupure)
    mock_time_reset();
    state_machine_init();
    avancer(1);  // mise à jour du statut

    machine_status_t s;
    state_machine_get_status(&s);
    TEST_ASSERT_TRUE(s.coupure_detectee);
    TEST_ASSERT_EQUAL_FLOAT(100.0f, s.longueur_deroulee_m);  // déroulé restauré
    TEST_ASSERT_EQUAL_FLOAT(20.0f,  s.longueur_enroulee_m);  // progression session restaurée

    // Même avec pression ok, la machine attend la décision de l'opérateur
    avancer(5);
    TEST_ASSERT_EQUAL_INT(ETAT_VEILLE, state_machine_get_etat());
}

// Test B — boot après urgence sauvegardée en NVS :
//   raison_arret persistée, démarrage automatique bloqué
static void test_boot_apres_urgence_nvs(void)
{
    config_nvs_sauver_urgence("Secu spires");

    mock_time_reset();
    state_machine_init();

    // raison_arret disponible immédiatement (charger_config_interne le recopie dans s_status)
    machine_status_t s;
    state_machine_get_status(&s);
    TEST_ASSERT_EQUAL_STRING("Secu spires", s.raison_arret);

    // Pas de démarrage automatique malgré conditions valides
    avancer(5);
    TEST_ASSERT_EQUAL_INT(ETAT_VEILLE, state_machine_get_etat());
}

// Test C — reprise automatique (reprise_auto_on=true) :
//   boot après coupure → transition directe VEILLE→OUVERTURE_CANON dès pression OK
static void test_reprise_auto_apres_coupure(void)
{
    config_machine_t m = CFG_MACHINE_DEFAUT;
    m.t_vidange_s     = 5.0f;
    m.reprise_auto_on = true;
    config_nvs_sauver_machine(&m);

    config_nvs_sauver_session_active(true);
    config_nvs_sauver_deroule(100.0f);
    config_nvs_sauver_longueur(250.0f);

    mock_time_reset();
    state_machine_init();

    // 1 tick → OUVERTURE_CANON (reprise auto, sans attendre l'opérateur)
    tick_state_machine();
    mock_time_advance_ms(100);
    TEST_ASSERT_EQUAL_INT(ETAT_OUVERTURE_CANON, state_machine_get_etat());
    ASSERT_EVS(true, false);  // canon ouvert, poumon fermé

    // Longueurs restaurées depuis NVS (ST1 Bis 330m : abs_start=230, session=20m)
    machine_status_t s;
    state_machine_get_status(&s);
    TEST_ASSERT_EQUAL_FLOAT(100.0f, s.longueur_deroulee_m);
    TEST_ASSERT_EQUAL_FLOAT(20.0f,  s.longueur_enroulee_m);
}

// Test D — reprise manuelle (reprise_auto_on=false) :
//   boot après coupure → machine attend décision opérateur (comportement actuel)
static void test_reprise_manuelle_apres_coupure(void)
{
    config_machine_t m = CFG_MACHINE_DEFAUT;
    m.t_vidange_s     = 5.0f;
    // reprise_auto_on = false (défaut)
    config_nvs_sauver_machine(&m);

    config_nvs_sauver_session_active(true);
    config_nvs_sauver_deroule(100.0f);
    config_nvs_sauver_longueur(250.0f);

    mock_time_reset();
    state_machine_init();

    // 1 tick → reste en VEILLE (attend intervention opérateur)
    tick_state_machine();
    mock_time_advance_ms(100);
    TEST_ASSERT_EQUAL_INT(ETAT_VEILLE, state_machine_get_etat());
    ASSERT_EVS(false, false);

    machine_status_t s;
    state_machine_get_status(&s);
    TEST_ASSERT_TRUE(s.coupure_detectee);
    TEST_ASSERT_EQUAL_FLOAT(100.0f, s.longueur_deroulee_m);
    TEST_ASSERT_EQUAL_FLOAT(20.0f,  s.longueur_enroulee_m);
}

void suite_scenario_reboot(void)
{
    unity_suite_setup(local_setUp, local_tearDown);
    RUN_TEST(test_coupure_courant_detectee);
    RUN_TEST(test_boot_apres_urgence_nvs);
    RUN_TEST(test_reprise_auto_apres_coupure);
    RUN_TEST(test_reprise_manuelle_apres_coupure);
}
