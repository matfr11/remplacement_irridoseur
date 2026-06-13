#include "sdkconfig.h"
#include "gpio_handler.h"
#include "esp_log.h"
#include <math.h>

// Tests unitaires GPIO handler — PR-02
// Exécutés au démarrage quand CONFIG_IRRI_ENABLE_TESTS=y (hors production)
// NOTE : le calcul de vitesse par fenêtre d'impulsions a été retiré (legacy) —
// les impulsions ne servent plus qu'à la mesure de longueur. La vitesse vient
// exclusivement du timing des cycles poumon (set_vitesse_estimee).
static const char *TAG = "test_gpio";

#define PASS(name)  ESP_LOGI(TAG, "PASS  %s", name)
#define FAIL(name, fmt, ...) ESP_LOGE(TAG, "FAIL  %s -- " fmt, name, ##__VA_ARGS__)

// Tolérance pour comparaisons float (m/h)
#define TOL_MH  0.5f

// Injecte N pulses équidistants séparés de interval_us microsecondes
static void injecter_pulses(int n, int64_t interval_us)
{
    int64_t t = 0;
    for (int i = 0; i < n; i++) {
        gpio_handler_test_injecter_pulse(t);
        t += interval_us;
    }
}

// =============================================================================
// Test 1 — Vitesse depuis cycles poumon — retourne vitesse estimée injectée
// =============================================================================
static void test_vitesse_depuis_cycles_poumon(void)
{
    gpio_handler_test_reset();
    float vitesse_estimee = 45.7f;
    gpio_handler_set_vitesse_depuis_cycles_poumon(true);
    gpio_handler_set_vitesse_estimee(vitesse_estimee);

    float v = gpio_get_vitesse_m_h();
    if (fabsf(v - vitesse_estimee) < TOL_MH) {
        PASS("vitesse_depuis_cycles_poumon");
    } else {
        FAIL("vitesse_depuis_cycles_poumon", "attendu=%.1f obtenu=%.1f", vitesse_estimee, v);
    }
}

// =============================================================================
// Test 2 — Mode cycles poumon désactivé — vitesse nulle (aucune autre source)
// =============================================================================
static void test_vitesse_mode_desactive(void)
{
    gpio_handler_test_reset();
    gpio_handler_set_vitesse_depuis_cycles_poumon(true);
    gpio_handler_set_vitesse_estimee(999.0f);

    gpio_handler_set_vitesse_depuis_cycles_poumon(false);

    float v = gpio_get_vitesse_m_h();
    if (fabsf(v) < TOL_MH) {
        PASS("vitesse_mode_desactive");
    } else {
        FAIL("vitesse_mode_desactive", "attendu=0.0 obtenu=%.1f", v);
    }
}

// =============================================================================
// Test 3 — Vitesse nulle après reset (état initial)
// =============================================================================
static void test_vitesse_apres_reset(void)
{
    gpio_handler_test_reset();

    float v = gpio_get_vitesse_m_h();
    if (fabsf(v) < TOL_MH) {
        PASS("vitesse_apres_reset");
    } else {
        FAIL("vitesse_apres_reset", "attendu=0.0 obtenu=%.1f", v);
    }
}

// =============================================================================
// Test 4 — Compteur impulsions et reset (mesure de longueur)
// =============================================================================
static void test_compteur_impulsions(void)
{
    gpio_handler_test_reset();
    injecter_pulses(7, 500000LL);

    uint32_t count = gpio_get_impulsions();
    if (count == 7) {
        PASS("compteur_impulsions_7");
    } else {
        FAIL("compteur_impulsions_7", "attendu=7 obtenu=%lu", (unsigned long)count);
    }

    gpio_reset_impulsions_cycle();
    count = gpio_get_impulsions();
    if (count == 0) {
        PASS("reset_impulsions");
    } else {
        FAIL("reset_impulsions", "attendu=0 obtenu=%lu", (unsigned long)count);
    }
}

// =============================================================================
// Test 5 — Lecture entrées (smoke test — valeurs dépendent du câblage réel)
// =============================================================================
static void test_lire_entrees(void)
{
    entrees_t e;
    gpio_handler_lire_entrees(&e);
    // Vérification que l'appel ne crash pas (struct bien remplie)
    // Les valeurs dépendent du matériel — on ne peut pas asserter ici
    ESP_LOGI(TAG, "lire_entrees: fin_course=%d secu_spires=%d poumon=%d pression=%d",
             e.fin_course, e.secu_spires, e.poumon_plein, e.pression_ok);
    PASS("lire_entrees_no_crash");
}

// =============================================================================
// Tests 6-9 — EVs bistables
// =============================================================================

static void test_ev_bistable_ouverture(void)
{
    gpio_handler_test_reset();
    if (!gpio_ev_canon_get()) {
        gpio_ev_canon_set(true);
        if (gpio_ev_canon_get()) {
            PASS("ev_bistable_ouverture");
        } else {
            FAIL("ev_bistable_ouverture", "etat attendu=ouvert obtenu=ferme");
        }
    } else {
        FAIL("ev_bistable_ouverture", "etat initial incorrect");
    }
}

static void test_ev_bistable_fermeture(void)
{
    gpio_handler_test_reset();
    gpio_ev_canon_set(true);
    gpio_ev_canon_set(false);
    if (!gpio_ev_canon_get()) {
        PASS("ev_bistable_fermeture");
    } else {
        FAIL("ev_bistable_fermeture", "etat attendu=ferme obtenu=ouvert");
    }
}

static void test_ev_bistable_idempotent(void)
{
    gpio_handler_test_reset();
    gpio_ev_poumon_set(true);
    gpio_ev_poumon_set(true);   // double commande — ne doit pas crasher
    if (gpio_ev_poumon_get()) {
        PASS("ev_bistable_idempotent");
    } else {
        FAIL("ev_bistable_idempotent", "etat perdu apres double commande");
    }
}

static void test_ev_all_off(void)
{
    gpio_handler_test_reset();
    gpio_ev_canon_set(true);
    gpio_ev_poumon_set(true);
    gpio_all_ev_off();
    if (!gpio_ev_canon_get() && !gpio_ev_poumon_get()) {
        PASS("ev_all_off");
    } else {
        FAIL("ev_all_off", "canon=%d poumon=%d (attendu 0/0)",
             gpio_ev_canon_get(), gpio_ev_poumon_get());
    }
}

// =============================================================================
// Point d'entrée
// =============================================================================
void test_gpio_run(void)
{
    ESP_LOGI(TAG, "=== Tests GPIO — PR-02 ===");

    test_vitesse_depuis_cycles_poumon();
    test_vitesse_mode_desactive();
    test_vitesse_apres_reset();
    test_compteur_impulsions();
    test_lire_entrees();
    test_ev_bistable_ouverture();
    test_ev_bistable_fermeture();
    test_ev_bistable_idempotent();
    test_ev_all_off();

    ESP_LOGI(TAG, "=== Fin tests GPIO ===");
}
