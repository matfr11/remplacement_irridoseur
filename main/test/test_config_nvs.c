#include "sdkconfig.h"
#include "config_nvs.h"
#include "esp_log.h"
#include <string.h>

// Tests unitaires config NVS — PR-07
// Compilé uniquement si CONFIG_IRRI_ENABLE_TESTS=y
// Les tests NVS tournent sur vrai hardware : toujours écrire avant de lire.
static const char *TAG = "test_nvs";

#define EPSILON 0.001f

#define PASS(name) ESP_LOGI(TAG, "PASS  %s", name)
#define FAIL(name, fmt, ...) ESP_LOGE(TAG, "FAIL  %s -- " fmt, name, ##__VA_ARGS__)

static void assert_near(float attendu, float reel, float eps, const char *nom)
{
    float diff = attendu - reel;
    if (diff < 0.0f) diff = -diff;
    if (diff <= eps) {
        PASS(nom);
    } else {
        FAIL(nom, "attendu=%.4f obtenu=%.4f", attendu, reel);
    }
}

static void assert_int_eq(int attendu, int reel, const char *nom)
{
    if (attendu == reel) {
        PASS(nom);
    } else {
        FAIL(nom, "attendu=%d obtenu=%d", attendu, reel);
    }
}

static void assert_bool(bool attendu, bool reel, const char *nom)
{
    if (attendu == reel) {
        PASS(nom);
    } else {
        FAIL(nom, "attendu=%d obtenu=%d", (int)attendu, (int)reel);
    }
}

// =============================================================================
// T01 — config_programme_est_valide : struct à zéro → false
// =============================================================================
static void test_valider_programme_vide(void)
{
    config_programme_t p;
    memset(&p, 0, sizeof(p));
    assert_bool(false, config_programme_est_valide(&p), "valider_programme_vide");
}

// =============================================================================
// T02 — config_programme_est_valide : struct valide → true
// =============================================================================
static void test_valider_programme_complet(void)
{
    config_programme_t p = {
        .nom = "Test",
        .dose_mm = 25.0f,
        .largeur_m = 18.0f,
        .buse_mm = 18,
        .pression_bar = 6.0f,
    };
    assert_bool(true, config_programme_est_valide(&p), "valider_programme_complet");
}

// =============================================================================
// T03 — config_programme_est_valide : buse_mm = 0, reste valide → false
// =============================================================================
static void test_valider_buse_zero(void)
{
    config_programme_t p = {
        .nom = "Test",
        .dose_mm = 25.0f,
        .largeur_m = 18.0f,
        .buse_mm = 0,
        .pression_bar = 6.0f,
    };
    assert_bool(false, config_programme_est_valide(&p), "valider_buse_zero");
}

// =============================================================================
// T04 — config_machine roundtrip
// =============================================================================
static void test_machine_roundtrip(void)
{
    config_machine_t cfg_out = CFG_MACHINE_DEFAUT;
    cfg_out.facteur_correction = 1.23f;
    cfg_out.kp_regulation      = 0.42f;
    cfg_out.fenetre_vitesse    = 7;
    cfg_out.cycles_par_tour    = 40.0f;

    esp_err_t ret = config_nvs_sauver_machine(&cfg_out);
    if (ret != ESP_OK) { FAIL("machine_roundtrip_save", "err=%d", ret); return; }

    config_machine_t cfg_in;
    ret = config_nvs_lire_machine(&cfg_in);
    if (ret != ESP_OK) { FAIL("machine_roundtrip_load", "err=%d", ret); return; }

    assert_near(1.23f,  cfg_in.facteur_correction, EPSILON, "machine_roundtrip_facteur");
    assert_near(0.42f,  cfg_in.kp_regulation,      EPSILON, "machine_roundtrip_kp");
    assert_int_eq(7,    cfg_in.fenetre_vitesse,             "machine_roundtrip_fenetre");
    assert_near(40.0f,  cfg_in.cycles_par_tour,    EPSILON, "machine_roundtrip_cycles_par_tour");
}

// =============================================================================
// T05 — programme idx=0 roundtrip
// =============================================================================
static void test_prog0_roundtrip(void)
{
    config_programme_t prog_out = {
        .nom            = "Mais",
        .dose_mm        = 30.0f,
        .largeur_m      = 20.0f,
        .buse_mm        = 20,
        .pression_bar   = 7.0f,
        .tempo_depart_on = true,
        .tempo_depart_s  = 60,
        .tempo_arrivee_on = false,
        .tempo_arrivee_s  = 0,
    };

    esp_err_t ret = config_nvs_sauver_programme(0, &prog_out);
    if (ret != ESP_OK) { FAIL("prog0_roundtrip_save", "err=%d", ret); return; }

    config_programme_t prog_in;
    ret = config_nvs_lire_programme(0, &prog_in);
    if (ret != ESP_OK) { FAIL("prog0_roundtrip_load", "err=%d", ret); return; }

    if (strcmp(prog_in.nom, "Mais") == 0) { PASS("prog0_roundtrip_nom"); }
    else { FAIL("prog0_roundtrip_nom", "obtenu='%s'", prog_in.nom); }

    assert_near(30.0f, prog_in.dose_mm,      EPSILON, "prog0_roundtrip_dose");
    assert_near(20.0f, prog_in.largeur_m,    EPSILON, "prog0_roundtrip_largeur");
    assert_int_eq(20,  prog_in.buse_mm,               "prog0_roundtrip_buse");
    assert_bool(true,  prog_in.tempo_depart_on,        "prog0_roundtrip_tdep_on");
    assert_int_eq(60,  prog_in.tempo_depart_s,         "prog0_roundtrip_tdep_s");
}

// =============================================================================
// T06 — programme idx=4 (dernier) roundtrip
// =============================================================================
static void test_prog4_roundtrip(void)
{
    config_programme_t prog_out = {
        .nom           = "Soja",
        .dose_mm       = 15.0f,
        .largeur_m     = 16.0f,
        .buse_mm       = 16,
        .pression_bar  = 5.5f,
    };

    esp_err_t ret = config_nvs_sauver_programme(4, &prog_out);
    if (ret != ESP_OK) { FAIL("prog4_roundtrip_save", "err=%d", ret); return; }

    config_programme_t prog_in;
    ret = config_nvs_lire_programme(4, &prog_in);
    if (ret != ESP_OK) { FAIL("prog4_roundtrip_load", "err=%d", ret); return; }

    if (strcmp(prog_in.nom, "Soja") == 0) { PASS("prog4_roundtrip_nom"); }
    else { FAIL("prog4_roundtrip_nom", "obtenu='%s'", prog_in.nom); }

    assert_near(15.0f, prog_in.dose_mm,   EPSILON, "prog4_roundtrip_dose");
    assert_near(5.5f,  prog_in.pression_bar, EPSILON, "prog4_roundtrip_pression");
}

// =============================================================================
// T07 — programme idx = -1 → ESP_ERR_INVALID_ARG
// =============================================================================
static void test_prog_idx_negatif(void)
{
    config_programme_t prog;
    esp_err_t ret = config_nvs_lire_programme(-1, &prog);
    if (ret == ESP_ERR_INVALID_ARG) { PASS("prog_idx_negatif"); }
    else { FAIL("prog_idx_negatif", "err=%d attendu=%d", ret, ESP_ERR_INVALID_ARG); }
}

// =============================================================================
// T08 — programme idx = 5 → ESP_ERR_INVALID_ARG
// =============================================================================
static void test_prog_idx_trop_grand(void)
{
    config_programme_t prog;
    esp_err_t ret = config_nvs_lire_programme(5, &prog);
    if (ret == ESP_ERR_INVALID_ARG) { PASS("prog_idx_trop_grand"); }
    else { FAIL("prog_idx_trop_grand", "err=%d attendu=%d", ret, ESP_ERR_INVALID_ARG); }
}

// =============================================================================
// T09 — prog_actif roundtrip
// =============================================================================
static void test_prog_actif_roundtrip(void)
{
    esp_err_t ret = config_nvs_sauver_prog_actif(3);
    if (ret != ESP_OK) { FAIL("prog_actif_save", "err=%d", ret); return; }

    int idx = -1;
    ret = config_nvs_lire_prog_actif(&idx);
    if (ret != ESP_OK) { FAIL("prog_actif_load", "err=%d", ret); return; }

    assert_int_eq(3, idx, "prog_actif_roundtrip");
}

// =============================================================================
// T10 — urgence roundtrip
// =============================================================================
static void test_urgence_roundtrip(void)
{
    esp_err_t ret = config_nvs_sauver_urgence("SEC-2 spires");
    if (ret != ESP_OK) { FAIL("urgence_save", "err=%d", ret); return; }

    char buf[64] = {0};
    ret = config_nvs_lire_urgence(buf, sizeof(buf));
    if (ret != ESP_OK) { FAIL("urgence_load", "err=%d", ret); return; }

    if (strcmp(buf, "SEC-2 spires") == 0) { PASS("urgence_roundtrip"); }
    else { FAIL("urgence_roundtrip", "obtenu='%s'", buf); }
}

// =============================================================================
// Point d'entrée
// =============================================================================
void test_config_nvs_run(void)
{
    ESP_LOGI(TAG, "=== Tests config_nvs ===");

    test_valider_programme_vide();
    test_valider_programme_complet();
    test_valider_buse_zero();
    test_machine_roundtrip();
    test_prog0_roundtrip();
    test_prog4_roundtrip();
    test_prog_idx_negatif();
    test_prog_idx_trop_grand();
    test_prog_actif_roundtrip();
    test_urgence_roundtrip();

    ESP_LOGI(TAG, "=== Fin tests config_nvs ===");
}
