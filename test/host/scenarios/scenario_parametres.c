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
#include <string.h>

// =============================================================================
// scenario_parametres.c — « Ce paramètre a un effet »
// Ajouté après la revue 2026-06 : deux paramètres configurables étaient morts
// (batt_warn_v — bug #10, fenetre_vitesse — retiré) et un troisième ignoré
// (t_rempl_fixe_s — bug #6, 20 s codé en dur) sans qu'aucun test ne le voie.
// Règle : chaque paramètre machine doit avoir un test prouvant un effet
// observable avec deux valeurs différentes.
//
// Non testés ici : kp_regulation / n_cycles_calib / dist_cycle_nvs (couverts
// par suite_regulation), batt_warn_v / batt_crit_v (couverts par
// suite_batterie), mode_deg_spires (couvert par scenario_modes_degrades),
// denivele_m (stocké mais pas encore implémenté — voir REGULATION.md).
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

// Réécrit la config machine et réinitialise la machine d'états
static void appliquer_machine(const config_machine_t *m)
{
    config_nvs_sauver_machine(m);
    state_machine_init();
}

// Amène la machine en EN_COURS / SOUS_VIDANGE (chrono sous-état armé proprement)
static void entrer_en_cours_vidange(void)
{
    state_machine_test_injecter_etat(ETAT_REMPLISSAGE_POUMON);
    gpio_set_level(PIN_POUMON_PLEIN, 1);
    avancer(1);
    gpio_set_level(PIN_POUMON_PLEIN, 0);
    TEST_ASSERT_EQUAL_INT(ETAT_EN_COURS, state_machine_get_etat());
}

// ── t_vidange_s : durée du SOUS_VIDANGE ──────────────────────────────────────

static void test_param_t_vidange_a_un_effet(void)
{
    machine_status_t st;

    // t_vidange = 2 s → après 2,5 s on a quitté SOUS_VIDANGE
    config_machine_t m = CFG_MACHINE_DEFAUT;
    m.t_vidange_s = 2.0f;
    appliquer_machine(&m);
    entrer_en_cours_vidange();
    avancer(25);
    state_machine_get_status(&st);
    TEST_ASSERT_NOT_EQUAL(SOUS_VIDANGE, st.sous_etat);

    // t_vidange = 8 s → après 2,5 s on y est toujours
    state_machine_test_reset();
    m.t_vidange_s = 8.0f;
    appliquer_machine(&m);
    entrer_en_cours_vidange();
    avancer(25);
    state_machine_get_status(&st);
    TEST_ASSERT_EQUAL_INT(SOUS_VIDANGE, st.sous_etat);
}

// ── t_rempl_fixe_s : timeout remplissage (bug #6 — était 20 s codé en dur) ──

static void test_param_t_rempl_fixe_a_un_effet(void)
{
    // Mode dégradé poumon : le remplissage initial se termine au timeout configuré
    config_machine_t m = CFG_MACHINE_DEFAUT;
    m.mode_deg_poumon = true;
    m.t_rempl_fixe_s  = 3.0f;
    appliquer_machine(&m);
    state_machine_test_injecter_etat(ETAT_REMPLISSAGE_POUMON);
    avancer(35);   // 3,5 s — sans jamais activer le contact poumon
    TEST_ASSERT_EQUAL_INT(ETAT_EN_COURS, state_machine_get_etat());

    // t_rempl_fixe = 8 s → après 3,5 s on remplit toujours
    state_machine_test_reset();
    m.t_rempl_fixe_s = 8.0f;
    appliquer_machine(&m);
    state_machine_test_injecter_etat(ETAT_REMPLISSAGE_POUMON);
    avancer(35);
    TEST_ASSERT_EQUAL_INT(ETAT_REMPLISSAGE_POUMON, state_machine_get_etat());
}

// ── fin_course_seuil_m : frontière fin normale / capteur suspect ────────────

static void test_param_fin_course_seuil_a_un_effet(void)
{
    // 5 m restants, seuil défaut 10 m → fin de bobine normale
    config_machine_t m = CFG_MACHINE_DEFAUT;
    appliquer_machine(&m);
    state_machine_test_set_longueurs(100.0f, 95.0f);
    TEST_ASSERT_TRUE(state_machine_fin_course_est_normale());

    // Même position, seuil 2 m → fin de course anormale (5 m > 2 m)
    m.fin_course_seuil_m = 2.0f;
    appliquer_machine(&m);
    state_machine_test_set_longueurs(100.0f, 95.0f);
    TEST_ASSERT_FALSE(state_machine_fin_course_est_normale());
}

// ── facteur_correction : étalonnage longueur (mesure DEROULE) ───────────────

static float mesurer_deroule_avec_facteur(float facteur)
{
    state_machine_test_reset();
    mock_time_reset();
    gpio_handler_test_reset();
    config_machine_t m = CFG_MACHINE_DEFAUT;
    m.facteur_correction = facteur;
    appliquer_machine(&m);

    state_machine_test_injecter_etat(ETAT_DEROULE);
    for (int i = 0; i < 10; i++)
        gpio_handler_test_injecter_pulse(esp_timer_get_time() + i * 1000);
    avancer(1);

    machine_status_t st;
    state_machine_get_status(&st);
    return st.mesure_deroule_m;
}

static void test_param_facteur_correction_a_un_effet(void)
{
    float m1 = mesurer_deroule_avec_facteur(1.0f);
    float m2 = mesurer_deroule_avec_facteur(2.0f);
    TEST_ASSERT_TRUE_MESSAGE(m1 > 0.1f, "mesure deroule attendue > 0");
    TEST_ASSERT_FLOAT_WITHIN(0.01f, m1 * 2.0f, m2);   // facteur ×2 → mesure ×2
}

// ── abaque_idx : sélection de l'abaque canon ─────────────────────────────────

static void test_param_abaque_idx_a_un_effet(void)
{
    machine_status_t st;

    config_machine_t m = CFG_MACHINE_DEFAUT;
    m.abaque_idx = 0;
    appliquer_machine(&m);
    state_machine_get_status(&st);
    TEST_ASSERT_NOT_NULL(strstr(st.abaque_nom, "150"));

    m.abaque_idx = 1;
    appliquer_machine(&m);
    state_machine_get_status(&st);
    TEST_ASSERT_NOT_NULL(strstr(st.abaque_nom, "100"));
}

// ── cycles_par_tour : valeur calibrée publiée, fallback profil si 0 ──────────

static void test_param_cycles_par_tour_a_un_effet(void)
{
    machine_status_t st;

    // Valeur calibrée 80 → reprise telle quelle dans le statut
    config_machine_t m = CFG_MACHINE_DEFAUT;
    m.cycles_par_tour = 80.0f;
    appliquer_machine(&m);
    avancer(1);   // cfg_cycles_par_tour publié dans le statut au tick
    state_machine_get_status(&st);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 80.0f, st.cfg_cycles_par_tour);

    // 0 (non calibré) → fallback volontaire sur la valeur du profil machine
    // (state_machine.c:157 — ST1bis : 40 cycles/tour), jamais 0 en pratique
    m.cycles_par_tour = 0.0f;
    appliquer_machine(&m);
    avancer(1);
    state_machine_get_status(&st);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 40.0f, st.cfg_cycles_par_tour);
}

// ── heartbeat_rc_on : oscillation PIN_HEARTBEAT (GPIO 23) ────────────────────

static void test_param_heartbeat_a_un_effet(void)
{
    // OFF (défaut) → le pin reste LOW
    config_machine_t m = CFG_MACHINE_DEFAUT;
    appliquer_machine(&m);
    bool vu_high = false;
    for (int i = 0; i < 15; i++) {
        tick_state_machine();
        mock_time_advance_ms(100);
        if (gpio_get_level(PIN_HEARTBEAT)) vu_high = true;
    }
    TEST_ASSERT_FALSE_MESSAGE(vu_high, "heartbeat OFF ne doit jamais lever GPIO 23");

    // ON → le pin bascule (période 500 ms)
    m.heartbeat_rc_on = true;
    appliquer_machine(&m);
    vu_high = false;
    for (int i = 0; i < 15; i++) {
        tick_state_machine();
        mock_time_advance_ms(100);
        if (gpio_get_level(PIN_HEARTBEAT)) vu_high = true;
    }
    TEST_ASSERT_TRUE_MESSAGE(vu_high, "heartbeat ON doit faire osciller GPIO 23");
}

void suite_scenario_parametres(void)
{
    unity_suite_setup(local_setUp, local_tearDown);
    RUN_TEST(test_param_t_vidange_a_un_effet);
    RUN_TEST(test_param_t_rempl_fixe_a_un_effet);
    RUN_TEST(test_param_fin_course_seuil_a_un_effet);
    RUN_TEST(test_param_facteur_correction_a_un_effet);
    RUN_TEST(test_param_abaque_idx_a_un_effet);
    RUN_TEST(test_param_cycles_par_tour_a_un_effet);
    RUN_TEST(test_param_heartbeat_a_un_effet);
}
