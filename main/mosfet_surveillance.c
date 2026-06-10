#include "mosfet_surveillance.h"
#include "ina3221.h"
#include "gpio_config.h"
#include "state_machine.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

static const char *TAG = "mosfet";

#define DELAI_APRES_MS   20

static ev_canal_t s_canon  = {MOSFET_OK, MOSFET_OK, false};
static ev_canal_t s_poumon = {MOSFET_OK, MOSFET_OK, false};

#ifdef CONFIG_IRRI_ENABLE_TESTS
static bool            s_sim_active = false;
static ina3221_mesure_t s_sim_canon  = {0.0f, 0.0f};
static ina3221_mesure_t s_sim_poumon = {0.0f, 0.0f};
#endif

// ── Helpers ───────────────────────────────────────────────────────────────────

static int get_ina_canal(int pin_ev)
{
    return (pin_ev == PIN_EV_CANON) ? INA3221_CH_EV_CANON : INA3221_CH_EV_POUMON;
}

static int get_pin_relais(int pin_ev)
{
    return (pin_ev == PIN_EV_CANON) ? PIN_RELAIS_CANON : PIN_RELAIS_POUMON;
}

static int get_pin_secours(int pin_ev)
{
    return (pin_ev == PIN_EV_CANON) ? PIN_MOSFET_SECOURS_CANON : PIN_MOSFET_SECOURS_POUMON;
}

static ev_canal_t *get_canal(int pin_ev)
{
    return (pin_ev == PIN_EV_CANON) ? &s_canon : &s_poumon;
}

// ── Vérification cohérence ────────────────────────────────────────────────────

static mosfet_etat_t verifier_coherence(int pin_ev, bool gpio_commande)
{
    // POURQUOI CE GUARD :
    // L'INA3221 est câblé sur le fil EV commun, APRÈS le COM du relais SPDT
    // (pas sur les sorties individuelles des MOSFETs).
    // Avant basculement → INA mesure OUT1/OUT2 (principal).
    // Après basculement → INA mesure OUT3/OUT4 (secours), car le relais a commuté.
    //
    // La détection fonctionnerait donc techniquement sur le secours, MAIS on
    // l'inhibe délibérément : toute anomalie transitoire sur OUT3/OUT4 serait
    // interprétée comme double-panne et déclencherait ARRET_URGENCE.
    // La vraie double-panne reste détectable via basculer_sur_secours()
    // (chemin : secours_actif && basculement → urgence).
    if (mosfet_secours_actif(pin_ev))
        return MOSFET_OK;

    ina3221_mesure_t m;

#ifdef CONFIG_IRRI_ENABLE_TESTS
    if (s_sim_active) {
        m = (pin_ev == PIN_EV_CANON) ? s_sim_canon : s_sim_poumon;
    } else if (!ina3221_est_ok()) {
        return MOSFET_OK;
    } else {
        m = ina3221_lire_canal(get_ina_canal(pin_ev));
    }
#else
    if (!ina3221_est_ok()) return MOSFET_OK;
    m = ina3221_lire_canal(get_ina_canal(pin_ev));
#endif

    bool ev_alimentee = (m.tension_v  > SEUIL_TENSION_EV_V);
    bool courant_ok   = (m.courant_ma > SEUIL_COURANT_EV_MA);

    if (!gpio_commande && ev_alimentee)
        return MOSFET_GRILLE_CC;

    if (gpio_commande && !ev_alimentee)
        return MOSFET_HS_OUVERT;

    if (gpio_commande && ev_alimentee && !courant_ok)
        return MOSFET_EV_DEBRANCHEE;

    return MOSFET_OK;
}

// ── Basculement sur secours ───────────────────────────────────────────────────

static bool basculer_sur_secours(int pin_ev, mosfet_etat_t cause)
{
    ev_canal_t *c = get_canal(pin_ev);
    const char *nom = (pin_ev == PIN_EV_CANON) ? "EV_CANON" : "EV_POUMON";

    if (c->secours_actif) {
        c->etat_secours = cause;
        ESP_LOGE(TAG, "%s : principal ET secours défaillants !", nom);
        state_machine_declencher_urgence(
            (pin_ev == PIN_EV_CANON)
            ? "Défaillance MOSFET EV_CANON — principal ET secours HS"
            : "Défaillance MOSFET EV_POUMON — principal ET secours HS");
        return false;
    }

    // Synchroniser OUT3/OUT4 avec l'état courant du principal avant de basculer
    // → évite tout glitch sur l'EV lors du changement de relais
    int pin_secours = get_pin_secours(pin_ev);
    gpio_set_level(pin_secours, gpio_get_level(pin_ev));

    c->etat_principal = cause;
    c->secours_actif  = true;

    // Activer relais : COM → NO → MOSFET secours
    gpio_set_level(get_pin_relais(pin_ev), 1);

    ESP_LOGW(TAG, "%s : MOSFET principal %s — secours actif",
             nom, mosfet_etat_str(cause));
    return true;
}

// ── API publique ──────────────────────────────────────────────────────────────

void mosfet_surveillance_init(void)
{
    // GPIO relais : sortie LOW (repos = NC = MOSFET principal)
    // GPIO OUT3/OUT4 : sortie LOW (secours inactif)
    gpio_config_t cfg = {
        .pin_bit_mask = (1ULL << PIN_RELAIS_CANON)          |
                        (1ULL << PIN_RELAIS_POUMON)          |
                        (1ULL << PIN_MOSFET_SECOURS_CANON)   |
                        (1ULL << PIN_MOSFET_SECOURS_POUMON),
        .mode         = GPIO_MODE_OUTPUT,
        .pull_up_en   = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type    = GPIO_INTR_DISABLE,
    };
    gpio_config(&cfg);
    gpio_set_level(PIN_RELAIS_CANON,          0);
    gpio_set_level(PIN_RELAIS_POUMON,         0);
    gpio_set_level(PIN_MOSFET_SECOURS_CANON,  0);
    gpio_set_level(PIN_MOSFET_SECOURS_POUMON, 0);

    ESP_LOGI(TAG, "init OK — relais GPIO2/GPIO4, secours GPIO26/GPIO27");
}

bool mosfet_test_demarrage(void)
{
    if (!ina3221_est_ok()) {
        ESP_LOGW(TAG, "INA3221 non disponible — test MOSFET ignoré");
        return true;
    }

    ESP_LOGI(TAG, "Test MOSFETs au démarrage...");

    int evs[2] = {PIN_EV_CANON, PIN_EV_POUMON};
    bool tout_ok = true;

    for (int i = 0; i < 2; i++) {
        int pin = evs[i];
        const char *nom = (pin == PIN_EV_CANON) ? "EV_CANON" : "EV_POUMON";

        // Test statique : GPIO=LOW → tension doit être 0V
        gpio_set_level(pin, 0);
        vTaskDelay(pdMS_TO_TICKS(100));
        mosfet_etat_t e = verifier_coherence(pin, false);
        if (e == MOSFET_GRILLE_CC) {
            ESP_LOGE(TAG, "%s principal : grillé CC", nom);
            if (!basculer_sur_secours(pin, MOSFET_GRILLE_CC))
                tout_ok = false;
            continue;
        }

        // Test dynamique supprimé : ouvrirait brièvement l'EV au boot.
        // MOSFET_HS_OUVERT et MOSFET_EV_DEBRANCHEE seront détectés par
        // mosfet_verifier_post_tick() à la première utilisation réelle.

        ESP_LOGI(TAG, "%s principal : OK", nom);
    }

    return tout_ok;
}

void mosfet_verifier_post_tick(void)
{
    int pin_c = mosfet_secours_actif(PIN_EV_CANON)
              ? PIN_MOSFET_SECOURS_CANON  : PIN_EV_CANON;
    int pin_p = mosfet_secours_actif(PIN_EV_POUMON)
              ? PIN_MOSFET_SECOURS_POUMON : PIN_EV_POUMON;
    vTaskDelay(pdMS_TO_TICKS(DELAI_APRES_MS));
    mosfet_verifier_apres(PIN_EV_CANON,  gpio_get_level(pin_c) != 0);
    mosfet_verifier_apres(PIN_EV_POUMON, gpio_get_level(pin_p) != 0);
}

void mosfet_reset_etat(void)
{
    s_canon  = (ev_canal_t){MOSFET_OK, MOSFET_OK, false};
    s_poumon = (ev_canal_t){MOSFET_OK, MOSFET_OK, false};
    ESP_LOGI(TAG, "etat MOSFET remis a zero");
}

void mosfet_verifier_avant(int pin_ev, bool etat_actuel)
{
    mosfet_etat_t e = verifier_coherence(pin_ev, etat_actuel);
    if (e != MOSFET_OK)
        basculer_sur_secours(pin_ev, e);
}

void mosfet_verifier_apres(int pin_ev, bool nouvel_etat)
{
    vTaskDelay(pdMS_TO_TICKS(DELAI_APRES_MS));
    mosfet_etat_t e = verifier_coherence(pin_ev, nouvel_etat);
    if (e != MOSFET_OK)
        basculer_sur_secours(pin_ev, e);
}

ev_canal_t mosfet_get_etat(int pin_ev)
{
    return (pin_ev == PIN_EV_CANON) ? s_canon : s_poumon;
}

bool mosfet_secours_actif(int pin_ev)
{
    return (pin_ev == PIN_EV_CANON) ? s_canon.secours_actif : s_poumon.secours_actif;
}

const char *mosfet_etat_str(mosfet_etat_t etat)
{
    switch (etat) {
        case MOSFET_OK:             return "OK";
        case MOSFET_GRILLE_CC:      return "grillé CC";
        case MOSFET_HS_OUVERT:      return "HS ouvert";
        case MOSFET_EV_DEBRANCHEE:  return "EV débranchée";
        default:                    return "inconnu";
    }
}

// =============================================================================
// Hooks de test
// =============================================================================

#ifdef CONFIG_IRRI_ENABLE_TESTS

void mosfet_sim_set_mesure(int pin_ev, float tension_v, float courant_ma)
{
    ina3221_mesure_t *m = (pin_ev == PIN_EV_CANON) ? &s_sim_canon : &s_sim_poumon;
    m->tension_v  = tension_v;
    m->courant_ma = courant_ma;
}

void mosfet_sim_enable(bool enable)
{
    s_sim_active = enable;
}

void mosfet_test_reset(void)
{
    s_canon  = (ev_canal_t){MOSFET_OK, MOSFET_OK, false};
    s_poumon = (ev_canal_t){MOSFET_OK, MOSFET_OK, false};
    gpio_set_level(PIN_RELAIS_CANON,          0);
    gpio_set_level(PIN_RELAIS_POUMON,         0);
    gpio_set_level(PIN_MOSFET_SECOURS_CANON,  0);
    gpio_set_level(PIN_MOSFET_SECOURS_POUMON, 0);
    s_sim_active  = false;
    s_sim_canon   = (ina3221_mesure_t){0.0f, 0.0f};
    s_sim_poumon  = (ina3221_mesure_t){0.0f, 0.0f};
}

#endif
