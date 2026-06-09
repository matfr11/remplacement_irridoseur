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

// Test A — défilement canon : fin_course HIGH→LOW → ETAT_DEROULE,
//           les impulsions accumulent la mesure, pression → démarrage automatique
static void test_deroule_puis_demarrage_auto(void)
{
    state_machine_test_injecter_etat(ETAT_VEILLE);
    // Fin de course active (bobine sur le terrain), pas encore de pression
    state_machine_test_set_fin_course(true);
    state_machine_test_set_pression(false);

    // Tick avec fin_course=HIGH : s_fc_deroule_prev=true, pas de transition DEROULE
    avancer(1);
    TEST_ASSERT_EQUAL_INT(ETAT_VEILLE, state_machine_get_etat());

    // Opérateur déroule le canon → fin_course passe LOW
    state_machine_test_set_fin_course(false);
    avancer(1);
    TEST_ASSERT_EQUAL_INT(ETAT_DEROULE, state_machine_get_etat());

    // Des pastilles passent devant le capteur vitesse pendant le défilement
    mock_gpio_inject_pulses(10, 5000);
    avancer(1);
    machine_status_t s;
    state_machine_get_status(&s);
    TEST_ASSERT_TRUE(s.mesure_deroule_m > 0.0f);

    // Opérateur retourne à la bobine, met le canon en pression → démarrage automatique
    state_machine_test_set_pression(true);
    avancer(1);
    TEST_ASSERT_EQUAL_INT(ETAT_OUVERTURE_CANON, state_machine_get_etat());
    state_machine_get_status(&s);
    TEST_ASSERT_TRUE(s.longueur_deroulee_m > 0.0f);
}

// Test B — correction de la longueur déroulée en cours d'arrosage
static void test_ajustement_longueur_en_cours(void)
{
    state_machine_test_injecter_etat(ETAT_EN_COURS);
    // Session déjà avancée : 100 m déroulés, 20 m enroulés
    state_machine_test_set_longueurs(100.0f, 20.0f);

    // L'opérateur corrige la longueur déroulée (mesure terrain plus précise)
    state_machine_cmd_set_longueur(80.0f);
    avancer(1);  // le statut est mis à jour à la fin de chaque tick

    machine_status_t s;
    state_machine_get_status(&s);
    TEST_ASSERT_EQUAL_FLOAT(80.0f, s.longueur_deroulee_m);
    TEST_ASSERT_EQUAL_FLOAT(0.0f,  s.longueur_enroulee_m);  // progression repart de 0
    TEST_ASSERT_EQUAL_INT(ETAT_EN_COURS, state_machine_get_etat());
}

// Test C — modification de programme en cours d'arrosage :
//           non prise en compte avant le retour en VEILLE
static void test_modification_programme_en_cours(void)
{
    state_machine_test_injecter_etat(ETAT_EN_COURS);

    machine_status_t s;
    state_machine_get_status(&s);
    TEST_ASSERT_EQUAL_FLOAT(20.0f, s.prog_dose_mm);   // programme initial (dose=20mm)

    // L'opérateur sauvegarde un nouveau programme pendant la session
    config_programme_t nouveau = {
        .nom          = "Nouveau",
        .dose_mm      = 35.0f,
        .largeur_m    = 18.0f,
        .buse_mm      = 25,
        .pression_bar = 3.5f,
    };
    config_nvs_sauver_programme(0, &nouveau);

    // Quelques ticks : le programme actif ne change pas en cours de session
    avancer(3);
    state_machine_get_status(&s);
    TEST_ASSERT_EQUAL_FLOAT(20.0f, s.prog_dose_mm);

    // Arrêt opérateur → ARRET_FINAL
    state_machine_cmd_stop();

    // handle_arret_final avec fin_course=false → recharge la config NVS et passe en VEILLE
    avancer(1);
    TEST_ASSERT_EQUAL_INT(ETAT_VEILLE, state_machine_get_etat());

    state_machine_get_status(&s);
    TEST_ASSERT_EQUAL_FLOAT(35.0f, s.prog_dose_mm);
    TEST_ASSERT_EQUAL_STRING("Nouveau", s.prog_nom);
}

void suite_scenario_operateur(void)
{
    unity_suite_setup(local_setUp, local_tearDown);
    RUN_TEST(test_deroule_puis_demarrage_auto);
    RUN_TEST(test_ajustement_longueur_en_cours);
    RUN_TEST(test_modification_programme_en_cours);
}
