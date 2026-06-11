#include "unity.h"

void unity_suite_setup(void (*su)(void), void (*td)(void));
#include "json_utils.h"
#include <string.h>

// =============================================================================
// test_json_utils.c — Parseur JSON minimal (extrait de webserver.c)
// Couvre notamment le bug #11 de la revue 2026-06 : espaces après ':'
// (clients tiers type Python json.dumps()).
// =============================================================================

// ── json_parse_float ──────────────────────────────────────────────────────────

static void test_json_float_nominal(void)
{
    float f = 0.0f;
    TEST_ASSERT_TRUE(json_parse_float("{\"dose_mm\":25.5}", "dose_mm", &f));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 25.5f, f);
}

static void test_json_float_espaces(void)
{
    float f = 0.0f;
    TEST_ASSERT_TRUE(json_parse_float("{\"dose_mm\":  25.5}", "dose_mm", &f));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 25.5f, f);
}

static void test_json_float_negatif(void)
{
    float f = 0.0f;
    TEST_ASSERT_TRUE(json_parse_float("{\"denivele_m\": -3.2}", "denivele_m", &f));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, -3.2f, f);
}

static void test_json_float_cle_absente(void)
{
    float f = 99.0f;
    TEST_ASSERT_FALSE(json_parse_float("{\"autre\":1.0}", "dose_mm", &f));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 99.0f, f);  // out intact
}

// Piège : "dose_mm" ne doit pas matcher la clé "dose_mm_max"
// (limitation connue : strstr matche les préfixes — ce test documente
// que la clé cherchée DOIT être suivie de ':' immédiatement)
static void test_json_float_cle_prefixe(void)
{
    float f = 0.0f;
    // "dose_mm": absent, seul "dose_mm_max": présent → la recherche de
    // "\"dose_mm\":" ne matche pas "\"dose_mm_max\":"
    TEST_ASSERT_FALSE(json_parse_float("{\"dose_mm_max\":50.0}", "dose_mm", &f));
}

// ── json_parse_int ────────────────────────────────────────────────────────────

static void test_json_int_nominal(void)
{
    int i = 0;
    TEST_ASSERT_TRUE(json_parse_int("{\"idx\":3}", "idx", &i));
    TEST_ASSERT_EQUAL_INT(3, i);
}

static void test_json_int_espaces(void)
{
    int i = 0;
    TEST_ASSERT_TRUE(json_parse_int("{\"idx\": 4}", "idx", &i));
    TEST_ASSERT_EQUAL_INT(4, i);
}

static void test_json_int_valeur_non_numerique(void)
{
    int i = 7;
    TEST_ASSERT_FALSE(json_parse_int("{\"idx\":\"abc\"}", "idx", &i));
    TEST_ASSERT_EQUAL_INT(7, i);
}

// ── json_parse_bool ───────────────────────────────────────────────────────────

static void test_json_bool_true_false(void)
{
    bool b = false;
    TEST_ASSERT_TRUE(json_parse_bool("{\"actif\":true}", "actif", &b));
    TEST_ASSERT_TRUE(b);
    TEST_ASSERT_TRUE(json_parse_bool("{\"actif\": false}", "actif", &b));
    TEST_ASSERT_FALSE(b);
}

static void test_json_bool_invalide(void)
{
    bool b = true;
    TEST_ASSERT_FALSE(json_parse_bool("{\"actif\":1}", "actif", &b));
    TEST_ASSERT_TRUE(b);  // out intact
}

// ── json_parse_string ─────────────────────────────────────────────────────────

static void test_json_string_nominal(void)
{
    char buf[21] = "inchange";
    json_parse_string("{\"nom\":\"Mais 2026\"}", "nom", buf, sizeof(buf));
    TEST_ASSERT_EQUAL_STRING("Mais 2026", buf);
}

// Bug #11 revue : espace après ':' (Python json.dumps)
static void test_json_string_espace_apres_deux_points(void)
{
    char buf[21] = "inchange";
    json_parse_string("{\"nom\": \"Soja\"}", "nom", buf, sizeof(buf));
    TEST_ASSERT_EQUAL_STRING("Soja", buf);
}

static void test_json_string_valeur_non_string(void)
{
    char buf[21] = "inchange";
    json_parse_string("{\"nom\": 42}", "nom", buf, sizeof(buf));
    TEST_ASSERT_EQUAL_STRING("inchange", buf);  // refusé, out intact
}

static void test_json_string_troncature(void)
{
    char buf[6];
    json_parse_string("{\"nom\":\"TropLongPourLeBuffer\"}", "nom", buf, sizeof(buf));
    TEST_ASSERT_EQUAL_STRING("TropL", buf);  // out_len-1 + '\0'
}

static void test_json_string_vide(void)
{
    char buf[21] = "inchange";
    json_parse_string("{\"nom\":\"\"}", "nom", buf, sizeof(buf));
    TEST_ASSERT_EQUAL_STRING("", buf);
}

// ── json_escape ───────────────────────────────────────────────────────────────

static void test_json_escape_guillemets_backslash(void)
{
    char buf[32];
    json_escape("a\"b\\c", buf, sizeof(buf));
    TEST_ASSERT_EQUAL_STRING("a\\\"b\\\\c", buf);
}

static void test_json_escape_troncature(void)
{
    char buf[4];
    json_escape("abcdef", buf, sizeof(buf));
    TEST_ASSERT_EQUAL_STRING("ab", buf);  // j < dst_len-2
}

// ── Point d'entrée ────────────────────────────────────────────────────────────

void suite_json_utils(void)
{
    unity_suite_setup(NULL, NULL);
    RUN_TEST(test_json_float_nominal);
    RUN_TEST(test_json_float_espaces);
    RUN_TEST(test_json_float_negatif);
    RUN_TEST(test_json_float_cle_absente);
    RUN_TEST(test_json_float_cle_prefixe);
    RUN_TEST(test_json_int_nominal);
    RUN_TEST(test_json_int_espaces);
    RUN_TEST(test_json_int_valeur_non_numerique);
    RUN_TEST(test_json_bool_true_false);
    RUN_TEST(test_json_bool_invalide);
    RUN_TEST(test_json_string_nominal);
    RUN_TEST(test_json_string_espace_apres_deux_points);
    RUN_TEST(test_json_string_valeur_non_string);
    RUN_TEST(test_json_string_troncature);
    RUN_TEST(test_json_string_vide);
    RUN_TEST(test_json_escape_guillemets_backslash);
    RUN_TEST(test_json_escape_troncature);
}
