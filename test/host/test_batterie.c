#include "unity.h"

void unity_suite_setup(void (*su)(void), void (*td)(void));
#include "batterie.h"
#include "gpio_config.h"
#include "mock_ina3221.h"

// =============================================================================
// test_batterie.c — Module batterie réel (était un mock vide avant 2026-06)
// Couvre le bug #10 de la revue : batt_warn_v configurable sans effet.
// =============================================================================

static void set_voltage(float v)
{
    mock_ina3221_set_ok(true);
    mock_ina3221_set_canal(INA3221_CH_BATTERIE, v, 0.0f);
}

static void reset_batterie(void)
{
    mock_ina3221_reset();
    batterie_set_seuils(BATT_V_FAIBLE_MIN, BATT_V_CRITIQUE_MIN); // défauts 11.5/11.0
    set_voltage(12.5f); // réarme s_last_valid_v à une valeur connue
    batterie_lire_voltage();
}

// ── INA absent : état Inconnue, pas de tension fabriquée ─────────────────────
// DOIT s'exécuter en premier : s_has_valid est un static du module batterie,
// il ne repasse jamais à false après la première lecture valide.

static void test_batt_sans_ina_etat_inconnue(void)
{
    mock_ina3221_reset();  // ok=false → lire_tension retourne 0.0
    batt_status_t s = batterie_get_status();
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, s.voltage_v);  // pas de 12.5V inventé
    TEST_ASSERT_EQUAL_INT(BATT_ETAT_INCONNUE, s.etat);
    TEST_ASSERT_EQUAL_INT(0, s.pourcentage);
}

// Après une première lecture valide, une lecture invalide retombe sur la
// dernière valeur valide (PR-19 bug 7) — plus jamais sur Inconnue
static void test_batt_inconnue_puis_valide_puis_invalide(void)
{
    set_voltage(12.3f);
    TEST_ASSERT_EQUAL_INT(BATT_ETAT_CORRECTE, batterie_get_status().etat);
    mock_ina3221_reset();  // INA "débranché" en cours de route
    batt_status_t s = batterie_get_status();
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 12.3f, s.voltage_v);
    TEST_ASSERT_EQUAL_INT(BATT_ETAT_CORRECTE, s.etat);
}

// ── États aux frontières (seuils par défaut warn=11.5, crit=11.0) ────────────

static void test_batt_etat_charge(void)
{
    reset_batterie();
    set_voltage(13.6f);
    TEST_ASSERT_EQUAL_INT(BATT_ETAT_CHARGE, batterie_get_status().etat);
}

static void test_batt_etat_pleine(void)
{
    reset_batterie();
    set_voltage(12.5f);
    TEST_ASSERT_EQUAL_INT(BATT_ETAT_PLEINE, batterie_get_status().etat);
}

static void test_batt_etat_correcte(void)
{
    reset_batterie();
    set_voltage(12.0f);
    TEST_ASSERT_EQUAL_INT(BATT_ETAT_CORRECTE, batterie_get_status().etat);
}

static void test_batt_etat_faible(void)
{
    reset_batterie();
    set_voltage(11.2f);
    TEST_ASSERT_EQUAL_INT(BATT_ETAT_FAIBLE, batterie_get_status().etat);
}

static void test_batt_etat_critique(void)
{
    reset_batterie();
    set_voltage(10.8f);
    TEST_ASSERT_EQUAL_INT(BATT_ETAT_CRITIQUE, batterie_get_status().etat);
}

// Frontières exactes : >= seuil → état supérieur
static void test_batt_frontieres_exactes(void)
{
    reset_batterie();
    set_voltage(11.5f);
    TEST_ASSERT_EQUAL_INT(BATT_ETAT_CORRECTE, batterie_get_status().etat);
    set_voltage(11.0f);
    TEST_ASSERT_EQUAL_INT(BATT_ETAT_FAIBLE, batterie_get_status().etat);
}

// ── Bug #10 : le seuil warn configurable DOIT avoir un effet ─────────────────

static void test_batt_seuil_warn_configurable_a_un_effet(void)
{
    reset_batterie();
    set_voltage(11.8f);
    TEST_ASSERT_EQUAL_INT(BATT_ETAT_CORRECTE, batterie_get_status().etat);

    // Remonter le seuil warn à 12.0 → la même tension devient FAIBLE
    batterie_set_seuils(12.0f, 11.0f);
    TEST_ASSERT_EQUAL_INT(BATT_ETAT_FAIBLE, batterie_get_status().etat);
}

static void test_batt_seuil_crit_configurable_a_un_effet(void)
{
    reset_batterie();
    set_voltage(11.2f);
    TEST_ASSERT_EQUAL_INT(BATT_ETAT_FAIBLE, batterie_get_status().etat);

    // Remonter le seuil crit à 11.3 → la même tension devient CRITIQUE
    batterie_set_seuils(11.5f, 11.3f);
    TEST_ASSERT_EQUAL_INT(BATT_ETAT_CRITIQUE, batterie_get_status().etat);
}

// ── Pourcentage : 0 % au seuil crit, 100 % à BATT_V_PCT_100 (12.6) ───────────

static void test_batt_pct_zero_au_seuil_crit(void)
{
    reset_batterie();
    set_voltage(11.0f);
    TEST_ASSERT_EQUAL_INT(0, batterie_get_status().pourcentage);
}

static void test_batt_pct_100_a_pleine_charge(void)
{
    reset_batterie();
    set_voltage(12.6f);
    TEST_ASSERT_EQUAL_INT(100, batterie_get_status().pourcentage);
}

static void test_batt_pct_borne_a_100(void)
{
    reset_batterie();
    set_voltage(13.8f);
    TEST_ASSERT_EQUAL_INT(100, batterie_get_status().pourcentage);
}

static void test_batt_pct_milieu(void)
{
    reset_batterie();
    set_voltage(11.8f);  // (11.8-11.0)/(12.6-11.0) = 50%
    TEST_ASSERT_EQUAL_INT(50, batterie_get_status().pourcentage);
}

// Le pourcentage suit le seuil crit configurable
static void test_batt_pct_suit_seuil_crit(void)
{
    reset_batterie();
    batterie_set_seuils(11.5f, 11.8f);
    set_voltage(11.8f);  // = seuil crit → 0%
    TEST_ASSERT_EQUAL_INT(0, batterie_get_status().pourcentage);
}

// ── Robustesse lecture (PR-19 bug 7 : retour sur dernière valeur valide) ─────

static void test_batt_tension_aberrante_retourne_derniere_valide(void)
{
    reset_batterie();
    set_voltage(12.1f);
    batterie_lire_voltage();          // mémorise 12.1
    set_voltage(28.0f);               // > BATT_V_MAX × 1.1 → aberrant
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 12.1f, batterie_lire_voltage());
}

static void test_batt_lecture_invalide_retourne_derniere_valide(void)
{
    reset_batterie();
    set_voltage(12.2f);
    batterie_lire_voltage();          // mémorise 12.2
    set_voltage(0.0f);                // < 0.5V → invalide (INA HS / câble)
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 12.2f, batterie_lire_voltage());
}

// ── Chaînes d'état (utilisées par le JSON statut et l'UI) ────────────────────

static void test_batt_etat_str(void)
{
    TEST_ASSERT_EQUAL_STRING("En charge", batterie_etat_str(BATT_ETAT_CHARGE));
    TEST_ASSERT_EQUAL_STRING("Pleine",    batterie_etat_str(BATT_ETAT_PLEINE));
    TEST_ASSERT_EQUAL_STRING("Correcte",  batterie_etat_str(BATT_ETAT_CORRECTE));
    TEST_ASSERT_EQUAL_STRING("Faible",    batterie_etat_str(BATT_ETAT_FAIBLE));
    TEST_ASSERT_EQUAL_STRING("Critique",  batterie_etat_str(BATT_ETAT_CRITIQUE));
    TEST_ASSERT_EQUAL_STRING("Inconnue",  batterie_etat_str(BATT_ETAT_INCONNUE));
    TEST_ASSERT_EQUAL_STRING("Inconnue",  batterie_etat_str((batt_etat_t)99));
}

static void test_batt_etat_couleur(void)
{
    // Codes hex UI : chaque état a une couleur distincte, fallback gris
    TEST_ASSERT_EQUAL_STRING("#3b82f6", batterie_etat_couleur(BATT_ETAT_CHARGE));
    TEST_ASSERT_EQUAL_STRING("#22c55e", batterie_etat_couleur(BATT_ETAT_PLEINE));
    TEST_ASSERT_EQUAL_STRING("#eab308", batterie_etat_couleur(BATT_ETAT_CORRECTE));
    TEST_ASSERT_EQUAL_STRING("#f97316", batterie_etat_couleur(BATT_ETAT_FAIBLE));
    TEST_ASSERT_EQUAL_STRING("#ef4444", batterie_etat_couleur(BATT_ETAT_CRITIQUE));
    TEST_ASSERT_EQUAL_STRING("#6b7280", batterie_etat_couleur(BATT_ETAT_INCONNUE));
    TEST_ASSERT_EQUAL_STRING("#6b7280", batterie_etat_couleur((batt_etat_t)99));
}

// ── Point d'entrée ────────────────────────────────────────────────────────────

void suite_batterie(void)
{
    unity_suite_setup(NULL, NULL);
    RUN_TEST(test_batt_sans_ina_etat_inconnue);        // doit rester en premier
    RUN_TEST(test_batt_inconnue_puis_valide_puis_invalide);
    RUN_TEST(test_batt_etat_charge);
    RUN_TEST(test_batt_etat_pleine);
    RUN_TEST(test_batt_etat_correcte);
    RUN_TEST(test_batt_etat_faible);
    RUN_TEST(test_batt_etat_critique);
    RUN_TEST(test_batt_frontieres_exactes);
    RUN_TEST(test_batt_seuil_warn_configurable_a_un_effet);
    RUN_TEST(test_batt_seuil_crit_configurable_a_un_effet);
    RUN_TEST(test_batt_pct_zero_au_seuil_crit);
    RUN_TEST(test_batt_pct_100_a_pleine_charge);
    RUN_TEST(test_batt_pct_borne_a_100);
    RUN_TEST(test_batt_pct_milieu);
    RUN_TEST(test_batt_pct_suit_seuil_crit);
    RUN_TEST(test_batt_tension_aberrante_retourne_derniere_valide);
    RUN_TEST(test_batt_lecture_invalide_retourne_derniere_valide);
    RUN_TEST(test_batt_etat_str);
    RUN_TEST(test_batt_etat_couleur);
}
