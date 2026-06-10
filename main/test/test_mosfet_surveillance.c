#include "sdkconfig.h"

#ifdef CONFIG_IRRI_ENABLE_TESTS

#include "mosfet_surveillance.h"
#include "gpio_config.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include <string.h>

// Tests unitaires surveillance MOSFETs — PR-19
// INA3221 simulé via mosfet_sim_set_mesure() / mosfet_sim_enable()
static const char *TAG = "test_mosfet";

#define PASS(name)          ESP_LOGI(TAG, "PASS  %s", name)
#define FAIL(name, fmt, ...) ESP_LOGE(TAG, "FAIL  %s -- " fmt, name, ##__VA_ARGS__)

// EV ON simulé : tension 12V, courant 900mA (MOSFET sain sous charge)
#define SIM_TENSION_ON      12.0f
#define SIM_COURANT_ON     900.0f
// EV OFF simulé : tension 0V, courant 0mA (MOSFET sain coupé)
#define SIM_TENSION_OFF      0.0f
#define SIM_COURANT_OFF      0.0f

// =============================================================================
// Test 1 — État initial : secours inactif, état OK sur les deux canaux
// =============================================================================
static void test_etat_initial(void)
{
    mosfet_test_reset();

    ev_canal_t c = mosfet_get_etat(PIN_EV_CANON);
    ev_canal_t p = mosfet_get_etat(PIN_EV_POUMON);

    if (!c.secours_actif && c.etat_principal == MOSFET_OK &&
        !p.secours_actif && p.etat_principal == MOSFET_OK) {
        PASS("etat_initial");
    } else {
        FAIL("etat_initial",
             "canon secours=%d etat=%d | poumon secours=%d etat=%d",
             c.secours_actif, c.etat_principal,
             p.secours_actif, p.etat_principal);
    }
}

// =============================================================================
// Test 2 — mosfet_etat_str() retourne les bonnes chaînes
// =============================================================================
static void test_etat_str(void)
{
    bool ok = strcmp(mosfet_etat_str(MOSFET_OK),            "OK")           == 0 &&
              strcmp(mosfet_etat_str(MOSFET_GRILLE_CC),     "grillé CC")    == 0 &&
              strcmp(mosfet_etat_str(MOSFET_HS_OUVERT),     "HS ouvert")    == 0 &&
              strcmp(mosfet_etat_str(MOSFET_EV_DEBRANCHEE), "EV débranchée") == 0;
    if (ok) PASS("etat_str");
    else    FAIL("etat_str", "une chaîne incorrecte");
}

// =============================================================================
// Test 3 — Sans INA3221 (ni simulation), verif_avant/après ne plantent pas
//           et n'activent pas le secours
// =============================================================================
static void test_verif_ina_indisponible(void)
{
    mosfet_test_reset();
    mosfet_sim_enable(false);   // INA3221 absent → MOSFET_OK systématique

    mosfet_verifier_avant(PIN_EV_CANON, false);
    mosfet_verifier_avant(PIN_EV_POUMON, true);

    if (!mosfet_secours_actif(PIN_EV_CANON) &&
        !mosfet_secours_actif(PIN_EV_POUMON)) {
        PASS("verif_ina_indisponible");
    } else {
        FAIL("verif_ina_indisponible", "basculement intempestif sans INA3221");
    }
}

// =============================================================================
// Test 4 — Détection court-circuit (MOSFET_GRILLE_CC)
// Tension haute alors que GPIO=LOW → basculement sur secours
// =============================================================================
static void test_detection_cc(void)
{
    mosfet_test_reset();
    mosfet_sim_enable(true);

    // Simuler MOSFET OUT1 grillé CC : tension 12V même GPIO=LOW
    mosfet_sim_set_mesure(PIN_EV_CANON, SIM_TENSION_ON, SIM_COURANT_ON);
    gpio_set_level(PIN_EV_CANON, 0);

    mosfet_verifier_avant(PIN_EV_CANON, false);

    ev_canal_t c = mosfet_get_etat(PIN_EV_CANON);
    if (c.secours_actif && c.etat_principal == MOSFET_GRILLE_CC) {
        PASS("detection_cc");
    } else {
        FAIL("detection_cc", "secours=%d etat_principal=%d",
             c.secours_actif, c.etat_principal);
    }
}

// =============================================================================
// Test 5 — Détection circuit ouvert (MOSFET_HS_OUVERT)
// Tension nulle alors que GPIO=HIGH → basculement sur secours
// =============================================================================
static void test_detection_hs_ouvert(void)
{
    mosfet_test_reset();
    mosfet_sim_enable(true);

    // Simuler MOSFET OUT2 HS ouvert : tension 0V même GPIO=HIGH
    mosfet_sim_set_mesure(PIN_EV_POUMON, SIM_TENSION_OFF, SIM_COURANT_OFF);
    gpio_set_level(PIN_EV_POUMON, 1);

    mosfet_verifier_apres(PIN_EV_POUMON, true);

    ev_canal_t p = mosfet_get_etat(PIN_EV_POUMON);
    if (p.secours_actif && p.etat_principal == MOSFET_HS_OUVERT) {
        PASS("detection_hs_ouvert");
    } else {
        FAIL("detection_hs_ouvert", "secours=%d etat_principal=%d",
             p.secours_actif, p.etat_principal);
    }
    gpio_set_level(PIN_EV_POUMON, 0);
}

// =============================================================================
// Test 6 — Détection EV débranchée (MOSFET_EV_DEBRANCHEE)
// Tension OK + courant nul alors que GPIO=HIGH
// =============================================================================
static void test_detection_ev_debranchee(void)
{
    mosfet_test_reset();
    mosfet_sim_enable(true);

    // Tension présente mais courant quasi nul (EV absente ou fil coupé côté EV)
    mosfet_sim_set_mesure(PIN_EV_CANON, SIM_TENSION_ON, 10.0f);
    gpio_set_level(PIN_EV_CANON, 1);

    mosfet_verifier_apres(PIN_EV_CANON, true);

    ev_canal_t c = mosfet_get_etat(PIN_EV_CANON);
    if (c.secours_actif && c.etat_principal == MOSFET_EV_DEBRANCHEE) {
        PASS("detection_ev_debranchee");
    } else {
        FAIL("detection_ev_debranchee", "secours=%d etat_principal=%d",
             c.secours_actif, c.etat_principal);
    }
    gpio_set_level(PIN_EV_CANON, 0);
}

// =============================================================================
// Test 7 — Vérification OK : tension et courant normaux → pas de basculement
// =============================================================================
static void test_verif_ok(void)
{
    mosfet_test_reset();
    mosfet_sim_enable(true);

    // EV alimentée normalement : 12V, 900mA, GPIO=HIGH
    mosfet_sim_set_mesure(PIN_EV_CANON, SIM_TENSION_ON, SIM_COURANT_ON);
    gpio_set_level(PIN_EV_CANON, 1);
    mosfet_verifier_apres(PIN_EV_CANON, true);

    // EV coupée normalement : 0V, 0mA, GPIO=LOW
    mosfet_sim_set_mesure(PIN_EV_POUMON, SIM_TENSION_OFF, SIM_COURANT_OFF);
    gpio_set_level(PIN_EV_POUMON, 0);
    mosfet_verifier_avant(PIN_EV_POUMON, false);

    if (!mosfet_secours_actif(PIN_EV_CANON) &&
        !mosfet_secours_actif(PIN_EV_POUMON)) {
        PASS("verif_ok_pas_de_basculement");
    } else {
        FAIL("verif_ok_pas_de_basculement",
             "basculement intempestif : canon=%d poumon=%d",
             mosfet_secours_actif(PIN_EV_CANON),
             mosfet_secours_actif(PIN_EV_POUMON));
    }
    gpio_set_level(PIN_EV_CANON, 0);
}

// =============================================================================
// Test 8 — Reset restaure l'état initial
// =============================================================================
static void test_reset_restaure_etat(void)
{
    // Forcer un basculement
    mosfet_test_reset();
    mosfet_sim_enable(true);
    mosfet_sim_set_mesure(PIN_EV_CANON, SIM_TENSION_ON, SIM_COURANT_ON);
    gpio_set_level(PIN_EV_CANON, 0);
    mosfet_verifier_avant(PIN_EV_CANON, false);

    // Vérifier que le secours est actif avant reset
    if (!mosfet_secours_actif(PIN_EV_CANON)) {
        FAIL("reset_restaure_etat", "secours non activé avant le reset");
        return;
    }

    // Reset
    mosfet_test_reset();

    ev_canal_t c = mosfet_get_etat(PIN_EV_CANON);
    if (!c.secours_actif && c.etat_principal == MOSFET_OK) {
        PASS("reset_restaure_etat");
    } else {
        FAIL("reset_restaure_etat", "secours=%d etat=%d apres reset",
             c.secours_actif, c.etat_principal);
    }
}

// =============================================================================
// Test 9 — mosfet_secours_actif() cohérent avec mosfet_get_etat()
// =============================================================================
static void test_secours_actif_coherent(void)
{
    mosfet_test_reset();
    mosfet_sim_enable(true);

    mosfet_sim_set_mesure(PIN_EV_POUMON, SIM_TENSION_OFF, SIM_COURANT_OFF);
    gpio_set_level(PIN_EV_POUMON, 1);
    mosfet_verifier_avant(PIN_EV_POUMON, true);

    bool actif_via_helper = mosfet_secours_actif(PIN_EV_POUMON);
    ev_canal_t p = mosfet_get_etat(PIN_EV_POUMON);

    if (actif_via_helper == p.secours_actif && actif_via_helper == true) {
        PASS("secours_actif_coherent");
    } else {
        FAIL("secours_actif_coherent",
             "helper=%d get_etat=%d", actif_via_helper, p.secours_actif);
    }
    gpio_set_level(PIN_EV_POUMON, 0);
    mosfet_test_reset();
}

// =============================================================================
// Point d'entrée
// =============================================================================
void test_mosfet_surveillance_run(void)
{
    ESP_LOGI(TAG, "=== Tests surveillance MOSFETs — PR-19 ===");

    // Init GPIO relais et secours (nécessaire avant les tests)
    mosfet_surveillance_init();

    test_etat_initial();
    test_etat_str();
    test_verif_ina_indisponible();
    test_detection_cc();
    test_detection_hs_ouvert();
    test_detection_ev_debranchee();
    test_verif_ok();
    test_reset_restaure_etat();
    test_secours_actif_coherent();

    // Nettoyage final
    mosfet_test_reset();
    mosfet_sim_enable(false);

    ESP_LOGI(TAG, "=== Fin tests surveillance MOSFETs ===");
}

#endif // CONFIG_IRRI_ENABLE_TESTS
