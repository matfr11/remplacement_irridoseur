#include "unity.h"
#include "unity_suite.h"
#include "config_nvs.h"
#include "machines/machines.h"
#include "mock_nvs.h"
#include <string.h>

// =============================================================================
// test_config_nvs.c (host) — Branches d'erreur et chemins de migration NVS.
// Ajouté après la revue 2026-06 : les chemins « clé absente », « blob d'une
// ancienne version firmware » et la validation des seuils batterie n'étaient
// couverts par aucun test host. machines.c (hors bornes, double entrée
// largeur/spires) était également orphelin.
// =============================================================================

static void local_setUp(void)    { mock_nvs_reset(); }
static void local_tearDown(void) {}

// ── Machine : défauts et migration ───────────────────────────────────────────

static void test_lire_machine_nvs_vierge_retourne_defauts(void)
{
    config_machine_t m;
    memset(&m, 0xFF, sizeof(m));
    TEST_ASSERT_EQUAL(ESP_OK, config_nvs_charger_machine(&m));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 2.0f,  m.t_vidange_s);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 1.0f,  m.facteur_correction);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 10.0f, m.fin_course_seuil_m);
    TEST_ASSERT_EQUAL_INT(0, m.abaque_idx);
}

static void test_lire_machine_blob_ancienne_version_retourne_defauts(void)
{
    // Blob "cfg" d'une taille différente (struct d'un ancien firmware)
    // → ESP_ERR_NVS_INVALID_LENGTH → retour silencieux aux défauts
    nvs_handle_t h;
    TEST_ASSERT_EQUAL(ESP_OK, nvs_open("irri_machine", NVS_READWRITE, &h));
    char vieux_blob[4] = {1, 2, 3, 4};
    TEST_ASSERT_EQUAL(ESP_OK, nvs_set_blob(h, "cfg", vieux_blob, sizeof(vieux_blob)));
    nvs_close(h);

    config_machine_t m;
    TEST_ASSERT_EQUAL(ESP_OK, config_nvs_charger_machine(&m));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 2.0f, m.t_vidange_s);   // défaut, pas le blob corrompu
}

// ── Seuils batterie : round-trip + validation à la lecture ───────────────────

static void test_batt_seuils_roundtrip(void)
{
    TEST_ASSERT_EQUAL(ESP_OK, config_nvs_sauver_batt_seuils(12.1f, 11.3f));
    float warn = 0.0f, crit = 0.0f;
    TEST_ASSERT_EQUAL(ESP_OK, config_nvs_lire_batt_seuils(&warn, &crit));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 12.1f, warn);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 11.3f, crit);
}

static void test_batt_seuils_hors_plage_ignores_a_la_lecture(void)
{
    // Valeurs absurdes persistées (hors [10, 15] V) → la lecture revient aux défauts
    TEST_ASSERT_EQUAL(ESP_OK, config_nvs_sauver_batt_seuils(20.0f, 5.0f));
    float warn = 0.0f, crit = 0.0f;
    TEST_ASSERT_EQUAL(ESP_OK, config_nvs_lire_batt_seuils(&warn, &crit));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 11.5f, warn);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 11.0f, crit);
}

// ── Stats campagne : reset ───────────────────────────────────────────────────

static void test_reset_stats_remet_tout_a_zero(void)
{
    config_stats_t s = {0};
    s.nb_sessions     = 7;
    s.total_volume_m3 = 42.0f;
    TEST_ASSERT_EQUAL(ESP_OK, config_nvs_sauver_stats(&s));

    TEST_ASSERT_EQUAL(ESP_OK, config_nvs_reset_stats());

    config_stats_t apres;
    TEST_ASSERT_EQUAL(ESP_OK, config_nvs_lire_stats(&apres));
    TEST_ASSERT_EQUAL_UINT32(0, apres.nb_sessions);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, apres.total_volume_m3);
}

// ── machines.c : index hors bornes et résolution double entrée ───────────────

static void test_machine_get_hors_bornes_retourne_premier(void)
{
    const machine_profile_t *premier = machine_get(0);
    TEST_ASSERT_NOT_NULL(premier);
    TEST_ASSERT_EQUAL_PTR(premier, machine_get(-1));
    TEST_ASSERT_EQUAL_PTR(premier, machine_get(99));
}

static void test_resoudre_double_entree(void)
{
    machine_profile_t p;

    // Largeur seule → spires calculées (largeur / d_ext)
    memset(&p, 0, sizeof(p));
    p.largeur_bobine_m = 1.5f;
    p.d_tuyau_ext_m    = 0.1f;
    TEST_ASSERT_TRUE(machine_resoudre_double_entree(&p));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 15.0f, p.spires_par_etage);

    // Spires seules → largeur calculée (spires × d_ext)
    memset(&p, 0, sizeof(p));
    p.spires_par_etage = 20.0f;
    p.d_tuyau_ext_m    = 0.1f;
    TEST_ASSERT_TRUE(machine_resoudre_double_entree(&p));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 2.0f, p.largeur_bobine_m);

    // Les deux fournis → conservés tels quels
    memset(&p, 0, sizeof(p));
    p.largeur_bobine_m = 1.5f;
    p.spires_par_etage = 14.0f;
    p.d_tuyau_ext_m    = 0.1f;
    TEST_ASSERT_TRUE(machine_resoudre_double_entree(&p));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 1.5f,  p.largeur_bobine_m);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 14.0f, p.spires_par_etage);

    // Les deux à 0 → profil invalide
    memset(&p, 0, sizeof(p));
    p.d_tuyau_ext_m = 0.1f;
    TEST_ASSERT_FALSE(machine_resoudre_double_entree(&p));
}

// ── Point d'entrée ────────────────────────────────────────────────────────────

void suite_config_nvs(void)
{
    unity_suite_setup(local_setUp, local_tearDown);
    RUN_TEST(test_lire_machine_nvs_vierge_retourne_defauts);
    RUN_TEST(test_lire_machine_blob_ancienne_version_retourne_defauts);
    RUN_TEST(test_batt_seuils_roundtrip);
    RUN_TEST(test_batt_seuils_hors_plage_ignores_a_la_lecture);
    RUN_TEST(test_reset_stats_remet_tout_a_zero);
    RUN_TEST(test_machine_get_hors_bornes_retourne_premier);
    RUN_TEST(test_resoudre_double_entree);
}
