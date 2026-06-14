#include "unity.h"
#include "unity_suite.h"
#include "state_machine.h"
#include "gpio_handler.h"
#include "gpio_config.h"
#include "config_nvs.h"
#include "calculs_hydraulique.h"
#include "abaques/abaques.h"
#include "mock_gpio.h"
#include "mock_nvs.h"
#include "mock_timer.h"
#include "mock_log.h"
#include "test_helpers.h"
#include <string.h>

// =============================================================================
// scenario_commandes_api.c — Commandes exposées par l'API WebSocket.
// Ajouté après la revue 2026-06 : cmd_start, cmd_etalonner, cmd_ev_*,
// cmd_start_deroule, cmd_reset_campagne, programme_preview, calc_vitesse,
// get_vitesse_max et set_time n'étaient couverts par AUCUN test — même profil
// de risque que les 11 bugs corrigés (code accessible depuis l'UI, jamais testé).
// =============================================================================

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

// ── cmd_start : requis après un stop opérateur ───────────────────────────────

static void test_cmd_start_requis_apres_stop_operateur(void)
{
    avancer(1);
    TEST_ASSERT_EQUAL_INT(ETAT_OUVERTURE_CANON, state_machine_get_etat());

    // Stop opérateur → ARRET_FINAL → VEILLE, et demarrage_autorise retombe
    state_machine_cmd_stop();
    avancer(1);
    TEST_ASSERT_EQUAL_INT(ETAT_VEILLE, state_machine_get_etat());

    // Pression toujours OK : la machine ne doit PAS redémarrer seule
    avancer(10);
    TEST_ASSERT_EQUAL_INT(ETAT_VEILLE, state_machine_get_etat());

    // cmd_start ré-autorise le départ
    state_machine_cmd_start();
    avancer(1);
    TEST_ASSERT_EQUAL_INT(ETAT_OUVERTURE_CANON, state_machine_get_etat());
}

// ── cmd_ev_canon/poumon : pilotage manuel uniquement en VEILLE ───────────────

static void test_cmd_ev_manuel_en_veille(void)
{
    TEST_ASSERT_EQUAL_INT(ETAT_VEILLE, state_machine_get_etat());

    state_machine_cmd_ev_canon_set(true);
    TEST_ASSERT_TRUE(gpio_ev_canon_get());
    state_machine_cmd_ev_canon_set(false);
    TEST_ASSERT_FALSE(gpio_ev_canon_get());

    state_machine_cmd_ev_poumon_set(true);
    TEST_ASSERT_TRUE(gpio_ev_poumon_get());
    state_machine_cmd_ev_poumon_set(false);
    TEST_ASSERT_FALSE(gpio_ev_poumon_get());
}

static void test_cmd_ev_ignore_hors_veille(void)
{
    // En urgence, EVs coupées — la commande manuelle doit être ignorée
    state_machine_test_injecter_etat(ETAT_ARRET_URGENCE);
    state_machine_cmd_ev_canon_set(true);
    state_machine_cmd_ev_poumon_set(true);
    TEST_ASSERT_FALSE(gpio_ev_canon_get());
    TEST_ASSERT_FALSE(gpio_ev_poumon_get());
}

// ── cmd_start_deroule : VEILLE → DEROULE, ignoré ailleurs ────────────────────

static void test_cmd_start_deroule(void)
{
    state_machine_cmd_start_deroule();
    TEST_ASSERT_EQUAL_INT(ETAT_DEROULE, state_machine_get_etat());

    // Stop depuis DEROULE → retour VEILLE direct (pas d'ARRET_FINAL)
    state_machine_cmd_stop();
    TEST_ASSERT_EQUAL_INT(ETAT_VEILLE, state_machine_get_etat());
}

static void test_cmd_start_deroule_ignore_en_cours(void)
{
    state_machine_test_injecter_etat(ETAT_EN_COURS);
    state_machine_cmd_start_deroule();
    TEST_ASSERT_EQUAL_INT(ETAT_EN_COURS, state_machine_get_etat());
}

// ── cmd_etalonner : facteur sauvegardé en NVS, garde-fous mécaniques ─────────

static void test_cmd_etalonner_sauve_facteur(void)
{
    // 100 m théoriques restaurés du NVS + 60 impulsions mesurées
    config_nvs_sauver_longueur(100.0f);
    state_machine_init();
    for (int i = 0; i < 60; i++)
        gpio_handler_test_injecter_pulse(esp_timer_get_time() + i * 1000);

    // Longueur réelle 110 m → facteur 1.10 accepté et persisté
    state_machine_cmd_etalonner(110.0f);
    config_machine_t m;
    TEST_ASSERT_EQUAL(ESP_OK, config_nvs_charger_machine(&m));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 1.10f, m.facteur_correction);
}

static void test_cmd_etalonner_refus_garde_fous(void)
{
    config_nvs_sauver_longueur(100.0f);
    state_machine_init();

    // Impulsions insuffisantes (< 50) → refus, facteur défaut conservé
    state_machine_cmd_etalonner(110.0f);
    config_machine_t m;
    config_nvs_charger_machine(&m);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 1.0f, m.facteur_correction);

    // Écart > 30 % (facteur 1.5) → refus aussi, même avec assez d'impulsions
    for (int i = 0; i < 60; i++)
        gpio_handler_test_injecter_pulse(esp_timer_get_time() + i * 1000);
    state_machine_cmd_etalonner(150.0f);
    config_nvs_charger_machine(&m);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 1.0f, m.facteur_correction);
}

// ── cmd_reset_campagne : stats NVS remises à zéro ────────────────────────────

static void test_cmd_reset_campagne(void)
{
    config_stats_t stats = {0};
    stats.nb_sessions     = 5;
    stats.total_volume_m3 = 123.0f;
    config_nvs_sauver_stats(&stats);

    state_machine_cmd_reset_campagne();

    config_stats_t apres;
    TEST_ASSERT_EQUAL(ESP_OK, config_nvs_lire_stats(&apres));
    TEST_ASSERT_EQUAL_UINT32(0, apres.nb_sessions);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, apres.total_volume_m3);
}

// ── set_time : synchro horloge visible dans le statut ────────────────────────

static void test_set_time_synchronise_statut(void)
{
    machine_status_t st;
    avancer(1);
    state_machine_get_status(&st);
    TEST_ASSERT_FALSE(st.heure_synchro);

    state_machine_set_time(1765000000);   // unix arbitraire
    avancer(1);
    state_machine_get_status(&st);
    TEST_ASSERT_TRUE(st.heure_synchro);
}

// ── calc_vitesse : délègue au lookup avec l'abaque actif ─────────────────────

static void test_calc_vitesse_coherent_avec_lookup(void)
{
    float debit_sm = 0.0f, p_buse_sm = 0.0f;
    float v_sm = state_machine_calc_vitesse(4.9f, 17.3f, 25.0f, 60.0f,
                                            &debit_sm, &p_buse_sm);
    float debit_ref = 0.0f, p_buse_ref = 0.0f;
    float v_ref = lookup_vitesse_cible(&ABAQUE_SR150C, 4.9f, 17.3f, 25.0f, 60.0f,
                                       &debit_ref, &p_buse_ref);
    TEST_ASSERT_TRUE(v_ref > 0.0f);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, v_ref, v_sm);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, debit_ref, debit_sm);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, p_buse_ref, p_buse_sm);

    // Paramètres invalides → 0, sorties remises à zéro
    debit_sm = 99.0f;
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f,
        state_machine_calc_vitesse(4.9f, 0.0f, 25.0f, 60.0f, &debit_sm, NULL));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, debit_sm);
}

// ── get_vitesse_max : dist_cycle / (t_rempl_min + t_vidange) ─────────────────

static void test_get_vitesse_max(void)
{
    // dist_cycle_nvs = 0 (défaut, non calibré) → pas de V max
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, state_machine_get_vitesse_max());

    // dist_cycle 0.4 m, t_vidange 2 s, t_rempl_min défaut 5 s
    // → V max = 0.4 / 7 × 3600 ≈ 205.7 m/h
    config_machine_t m = CFG_MACHINE_DEFAUT;
    m.dist_cycle_nvs = 0.4f;
    config_nvs_sauver_machine(&m);
    state_machine_init();
    TEST_ASSERT_FLOAT_WITHIN(1.0f, 205.7f, state_machine_get_vitesse_max());
}

// ── programme_preview : agrégat complet pour le formulaire programme ─────────

static void test_programme_preview_nominal(void)
{
    float esp = calcul_esp_nominal_m(&ABAQUE_SR150C, 4.9f, 17.3f);
    programme_preview_t pr = state_machine_programme_preview(4.9f, 17.3f, 25.0f, esp);

    TEST_ASSERT_TRUE_MESSAGE(pr.vitesse_m_h > 0.0f, "vitesse preview attendue > 0");
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 3.5f, pr.p_buse_bar);          // hit exact table
    TEST_ASSERT_FLOAT_WITHIN(0.1f, esp, pr.esp_nominal_m);
    TEST_ASSERT_FLOAT_WITHIN(0.1f, esp / 1.55f, pr.portee_m);      // esp_factor SR150C
    TEST_ASSERT_FLOAT_WITHIN(0.01f, esp * 0.75f, pr.esp_pos_min);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, esp * 1.10f, pr.esp_pos_max);

    // Bornes SR150C : p [4.9, 9.5] × 0.75/1.25, dose [15, 40] × 0.75/1.25
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 4.9f * 0.75f, pr.p_min);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 9.5f * 1.25f, pr.p_max);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 11.25f, pr.dose_min);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 50.0f,  pr.dose_max);

    // Programme nominal → aucun warning ; dist_cycle=0 → pas de limite vitesse
    TEST_ASSERT_FALSE(pr.w_pression_basse || pr.w_pression_haute
                      || pr.w_buse_hors_plage || pr.w_dose_hors_plage
                      || pr.w_esp_pos_chevauchement || pr.w_esp_pos_insuf);
    TEST_ASSERT_FALSE(pr.w_vitesse_limite);

    // Cohérence débit : debit_ls = m3/h / 3.6
    float debit_m3h = 0.0f;
    lookup_vitesse_cible(&ABAQUE_SR150C, 4.9f, 17.3f, 25.0f, esp, &debit_m3h, NULL);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, debit_m3h / 3.6f, pr.debit_ls);
}

static void test_programme_preview_invalide(void)
{
    // buse = 0 → struct entièrement à zéro, pas de crash
    programme_preview_t pr = state_machine_programme_preview(4.9f, 0.0f, 25.0f, 60.0f);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, pr.vitesse_m_h);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, pr.p_min);
    TEST_ASSERT_FALSE(pr.w_vitesse_limite);
}

static void test_programme_preview_warning_vitesse_limite(void)
{
    // dist_cycle court → V max ≈ 205.7 m/h ; dose 15 + largeur mini → V cible élevée
    config_machine_t m = CFG_MACHINE_DEFAUT;
    m.dist_cycle_nvs = 0.05f;   // V max = 0.05/7×3600 ≈ 25.7 m/h
    config_nvs_sauver_machine(&m);
    state_machine_init();

    programme_preview_t pr = state_machine_programme_preview(8.2f, 17.8f, 15.0f, 30.0f);
    TEST_ASSERT_TRUE_MESSAGE(pr.vitesse_m_h > pr.v_max_m_h,
                             "preconditions: V cible doit depasser V max");
    TEST_ASSERT_TRUE(pr.w_vitesse_limite);
}

void suite_scenario_commandes_api(void)
{
    unity_suite_setup(local_setUp, local_tearDown);
    RUN_TEST(test_cmd_start_requis_apres_stop_operateur);
    RUN_TEST(test_cmd_ev_manuel_en_veille);
    RUN_TEST(test_cmd_ev_ignore_hors_veille);
    RUN_TEST(test_cmd_start_deroule);
    RUN_TEST(test_cmd_start_deroule_ignore_en_cours);
    RUN_TEST(test_cmd_etalonner_sauve_facteur);
    RUN_TEST(test_cmd_etalonner_refus_garde_fous);
    RUN_TEST(test_cmd_reset_campagne);
    RUN_TEST(test_set_time_synchronise_statut);
    RUN_TEST(test_calc_vitesse_coherent_avec_lookup);
    RUN_TEST(test_get_vitesse_max);
    RUN_TEST(test_programme_preview_nominal);
    RUN_TEST(test_programme_preview_invalide);
    RUN_TEST(test_programme_preview_warning_vitesse_limite);
}
