#include "unity.h"
#include "unity_suite.h"
#include "state_machine.h"
#include "gpio_handler.h"
#include "gpio_config.h"
#include "config_nvs.h"
#include "mock_gpio.h"
#include "mock_nvs.h"
#include "mock_timer.h"
#include "mock_log.h"
#include "test_helpers.h"

// =============================================================================
// scenario_bilan_session.c — Vérifie les RÉSULTATS d'une session, pas seulement
// les transitions d'états. Ajouté après la revue 2026-06 : les bugs #5 (bilan
// volume/dose = 0), #7 (duree_s continue en VEILLE) et #8 (deroule_m NVS non
// remis) étaient invisibles car aucun scénario n'assertait les valeurs finales.
// =============================================================================

static void set_pression(bool ok)
{
    state_machine_test_set_pression(ok);
    gpio_set_level(PIN_PRESSOSTAT, ok ? 0 : 1);
}

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

// Démarre une session et amène la machine en EN_COURS avec des longueurs réalistes
static void demarrer_session_en_cours(void)
{
    avancer(1);   // VEILLE → OUVERTURE_CANON
    TEST_ASSERT_EQUAL_INT(ETAT_OUVERTURE_CANON, state_machine_get_etat());
    avancer(31);  // pression stable → REMPLISSAGE_POUMON
    TEST_ASSERT_EQUAL_INT(ETAT_REMPLISSAGE_POUMON, state_machine_get_etat());

    // Longueurs réalistes : 100 m déployés, 80 m de progression session
    // → surface = 80 × 18 m (largeur programme) = 1440 m²
    state_machine_test_set_longueurs(100.0f, 80.0f);

    gpio_set_level(PIN_POUMON_PLEIN, 1);
    avancer(1);
    gpio_set_level(PIN_POUMON_PLEIN, 0);
    TEST_ASSERT_EQUAL_INT(ETAT_EN_COURS, state_machine_get_etat());
}

// ── Bilan d'une session sans pause ────────────────────────────────────────────

static void test_bilan_volume_dose_coherents(void)
{
    demarrer_session_en_cours();
    avancer(100);  // 10 s d'arrosage

    machine_status_t st;
    state_machine_get_status(&st);
    TEST_ASSERT_TRUE_MESSAGE(st.debit_m3h > 0.1f, "debit abaque attendu > 0");
    TEST_ASSERT_FLOAT_WITHIN(0.1f, 1440.0f, st.surface_m2);

    state_machine_cmd_stop();
    TEST_ASSERT_EQUAL_INT(ETAT_ARRET_FINAL, state_machine_get_etat());
    avancer(1);    // bilan + sortie vers VEILLE (fin_course inactif)

    session_summary_t bilan;
    state_machine_get_session_summary(&bilan);

    // Bug #5 : volume = débit abaque × durée effective — JAMAIS 0 après une session
    // (le chrono session démarre à la stabilisation pression, sortie d'OUVERTURE_CANON :
    //  1 tick poumon + 100 ticks EN_COURS + stop + bilan ≈ 10 s)
    TEST_ASSERT_TRUE_MESSAGE(bilan.duree_s >= 10, "duree_s session attendue >= 10 s");
    TEST_ASSERT_EQUAL_INT(0, bilan.duree_pause_pression_s);
    float vol_attendu = st.debit_m3h * (float)bilan.duree_s / 3600.0f;
    TEST_ASSERT_FLOAT_WITHIN(0.01f, vol_attendu, bilan.volume_m3);
    TEST_ASSERT_TRUE_MESSAGE(bilan.volume_m3 > 0.0f, "volume bilan = 0 (bug #5)");

    // dose = volume / surface × 1000
    float dose_attendue = vol_attendu / 1440.0f * 1000.0f;
    TEST_ASSERT_FLOAT_WITHIN(0.01f, dose_attendue, bilan.dose_moy_mm);
    TEST_ASSERT_TRUE_MESSAGE(bilan.dose_moy_mm > 0.0f, "dose bilan = 0 (bug #5)");

    TEST_ASSERT_FLOAT_WITHIN(0.1f, 1440.0f, bilan.surface_m2);
}

// ── Bug #7 : duree_s figée après la fin de session ───────────────────────────

static void test_duree_figee_apres_arret_final(void)
{
    demarrer_session_en_cours();
    avancer(50);
    state_machine_cmd_stop();
    avancer(1);    // bilan + retour VEILLE
    TEST_ASSERT_EQUAL_INT(ETAT_VEILLE, state_machine_get_etat());

    session_summary_t bilan;
    state_machine_get_session_summary(&bilan);

    // 30 s s'écoulent en VEILLE → duree_s ne doit PAS continuer à compter
    avancer(300);
    machine_status_t st;
    state_machine_get_status(&st);
    TEST_ASSERT_EQUAL_INT(bilan.duree_s, st.duree_s);
}

// ── Bug #8 : NVS remis à zéro après arrêt propre ─────────────────────────────

static void test_nvs_remis_a_zero_apres_arret_final(void)
{
    demarrer_session_en_cours();
    avancer(50);
    state_machine_cmd_stop();
    avancer(1);

    float deroule = -1.0f, longueur = -1.0f;
    TEST_ASSERT_EQUAL(ESP_OK, config_nvs_lire_deroule(&deroule));
    TEST_ASSERT_EQUAL(ESP_OK, config_nvs_lire_longueur(&longueur));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, deroule);   // bug #8 : restait à l'ancienne valeur
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, longueur);
    TEST_ASSERT_FALSE(config_nvs_lire_session_active());
}

// ── Stats campagne incrémentées une seule fois ───────────────────────────────

static void test_stats_campagne_incrementees(void)
{
    config_stats_t avant;
    config_nvs_lire_stats(&avant);

    demarrer_session_en_cours();
    avancer(100);
    state_machine_cmd_stop();
    avancer(10);   // plusieurs ticks en VEILLE — le bilan ne doit compter qu'UNE session

    config_stats_t apres;
    config_nvs_lire_stats(&apres);
    TEST_ASSERT_EQUAL_UINT32(avant.nb_sessions + 1, apres.nb_sessions);
    TEST_ASSERT_TRUE_MESSAGE(apres.total_volume_m3 > avant.total_volume_m3,
                             "volume campagne non incremente");
    TEST_ASSERT_TRUE_MESSAGE(apres.total_surface_ha > avant.total_surface_ha,
                             "surface campagne non incrementee");
    TEST_ASSERT_TRUE_MESSAGE(apres.total_duree_h > avant.total_duree_h,
                             "duree campagne non incrementee");
}

// ── Bilan avec pause pression : la pause est exclue du volume ────────────────

static void test_bilan_exclut_pause_pression(void)
{
    demarrer_session_en_cours();
    avancer(50);   // 5 s d'arrosage

    // Pause pression de 20 s
    set_pression(false);
    avancer(1);
    TEST_ASSERT_EQUAL_INT(ETAT_PAUSE_PRESSION, state_machine_get_etat());
    avancer(200);

    // Reprise → OUVERTURE_CANON → REMPLISSAGE → EN_COURS
    set_pression(true);
    avancer(1);
    TEST_ASSERT_EQUAL_INT(ETAT_OUVERTURE_CANON, state_machine_get_etat());
    avancer(30);
    gpio_set_level(PIN_POUMON_PLEIN, 1);
    avancer(1);
    gpio_set_level(PIN_POUMON_PLEIN, 0);
    TEST_ASSERT_EQUAL_INT(ETAT_EN_COURS, state_machine_get_etat());
    avancer(50);

    machine_status_t st;
    state_machine_get_status(&st);

    state_machine_cmd_stop();
    avancer(1);

    session_summary_t bilan;
    state_machine_get_session_summary(&bilan);

    // ~20 s de pause comptabilisées
    TEST_ASSERT_INT_WITHIN(2, 20, bilan.duree_pause_pression_s);
    // Volume calculé sur la durée HORS pause
    float duree_irrig_s = (float)(bilan.duree_s - bilan.duree_pause_pression_s);
    float vol_attendu   = st.debit_m3h * duree_irrig_s / 3600.0f;
    TEST_ASSERT_FLOAT_WITHIN(0.01f, vol_attendu, bilan.volume_m3);
}

void suite_scenario_bilan_session(void)
{
    unity_suite_setup(local_setUp, local_tearDown);
    RUN_TEST(test_bilan_volume_dose_coherents);
    RUN_TEST(test_duree_figee_apres_arret_final);
    RUN_TEST(test_nvs_remis_a_zero_apres_arret_final);
    RUN_TEST(test_stats_campagne_incrementees);
    RUN_TEST(test_bilan_exclut_pause_pression);
}
