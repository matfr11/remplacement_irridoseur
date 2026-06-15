#include "gpio_handler.h"
#include "gpio_config.h"
#include "driver/gpio.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

#ifdef CONFIG_IRRI_TEST_MODE
  #include "simulator/simulator.h"
  #define READ_GPIO(pin)  sim_gpio_get_level(pin)
#else
  #define READ_GPIO(pin)  gpio_get_level(pin)
#endif

static const char *TAG = "gpio_handler";

static portMUX_TYPE s_mux = portMUX_INITIALIZER_UNLOCKED;

// Compteur d'impulsions capteur — sert uniquement à la mesure de longueur
// (déroulé en ETAT_DEROULE, progression enroulée par cycle).
static volatile uint32_t s_impulsions  = 0;
static volatile int64_t  s_last_isr_us = 0;

// Vitesse d'enroulement estimée depuis le timing des cycles poumon
// (fournie par state_machine chaque tick quand cycles_par_tour est calibré).
// C'est la SEULE source de vitesse — l'ancien calcul par fenêtre glissante
// d'impulsions a été retiré (jamais alimenté en production).
static bool  s_vitesse_depuis_cycles_poumon = false;
static float s_vitesse_estimee_mh = 0.0f;

// État logique des EVs bistables — mémorisé en RAM (pas lisible sur GPIO
// car bistable = pas de courant en régime permanent).
static bool s_ev_canon_ouverte  = false;
static bool s_ev_poumon_ouverte = false;

// Inversion contacts secs — configurable depuis NVS via gpio_handler_set_contacts_inv()
static bool s_fc_inv         = false;
static bool s_spires_inv     = false;
static bool s_poumon_inv     = false;
static bool s_pressostat_inv = false;

// =============================================================================
// ISR
// =============================================================================

static void IRAM_ATTR isr_capteur_vitesse(void *arg)
{
    (void)arg;
    int64_t now = esp_timer_get_time();
    portENTER_CRITICAL_ISR(&s_mux);
    if (now - s_last_isr_us < (int64_t)DEBOUNCE_VITESSE_MS * 1000) {
        portEXIT_CRITICAL_ISR(&s_mux);
        return;
    }
    s_last_isr_us = now;
    s_impulsions++;
    portEXIT_CRITICAL_ISR(&s_mux);
}

// =============================================================================
// Init
// =============================================================================

void gpio_handler_init(void)
{
    // Sorties EV bistables — OUT1/OUT2 (OUVRIR) + OUT3/OUT4 (FERMER)
    gpio_config_t out_cfg = {
        .pin_bit_mask = (1ULL << PIN_EV_CANON_OUVRIR)  |
                        (1ULL << PIN_EV_POUMON_OUVRIR)  |
                        (1ULL << PIN_EV_CANON_FERMER)   |
                        (1ULL << PIN_EV_POUMON_FERMER),
        .mode         = GPIO_MODE_INPUT_OUTPUT,
        .pull_up_en   = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type    = GPIO_INTR_DISABLE,
    };
    gpio_config(&out_cfg);
    gpio_set_level(PIN_EV_CANON_OUVRIR,  0);
    gpio_set_level(PIN_EV_POUMON_OUVRIR, 0);
    gpio_set_level(PIN_EV_CANON_FERMER,  0);
    gpio_set_level(PIN_EV_POUMON_FERMER, 0);

    // Entrées — résistances pull-up externes, pas de pull interne
    gpio_config_t in_cfg = {
        .pin_bit_mask = (1ULL << PIN_CAPTEUR_VITESSE) |
                        (1ULL << PIN_FIN_COURSE)       |
                        (1ULL << PIN_SECU_SPIRES)      |
                        (1ULL << PIN_POUMON_PLEIN)     |
                        (1ULL << PIN_PRESSOSTAT),
        .mode         = GPIO_MODE_INPUT,
        .pull_up_en   = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type    = GPIO_INTR_DISABLE,
    };
    gpio_config(&in_cfg);

    // REGLE UNIFIEE (decision 2026-06-11) : aucune pull interne — toutes les
    // entrees exigent leur pull-up externe 10k. Sans elle, la broche flotte :
    // un fil coupe peut lire 0 (= "pression OK", "pas de debordement"...)
    // au lieu de declencher la securite.

    gpio_set_intr_type(PIN_CAPTEUR_VITESSE, GPIO_INTR_POSEDGE);
    gpio_install_isr_service(0);
    gpio_isr_handler_add(PIN_CAPTEUR_VITESSE, isr_capteur_vitesse, NULL);

    // Impulsions FERMER au boot — garantit un état mécanique connu
    // (EVs bistables conservent leur position après coupure d'alimentation)
    gpio_all_ev_off();

    ESP_LOGI(TAG, "GPIO init — CANON OUVRIR=%d FERMER=%d | POUMON OUVRIR=%d FERMER=%d",
             PIN_EV_CANON_OUVRIR, PIN_EV_CANON_FERMER,
             PIN_EV_POUMON_OUVRIR, PIN_EV_POUMON_FERMER);
}

// =============================================================================
// Entrées
// =============================================================================

void gpio_handler_lire_entrees(entrees_t *entrees)
{
    bool fc_raw = READ_GPIO(PIN_FIN_COURSE)   != 0;
    bool sp_raw = READ_GPIO(PIN_SECU_SPIRES)  != 0;
    bool pp_raw = READ_GPIO(PIN_POUMON_PLEIN) != 0;
    bool ps_raw = READ_GPIO(PIN_PRESSOSTAT)   == 0;  // LOW=actif par défaut (NO)
    entrees->fin_course   = s_fc_inv         ? !fc_raw : fc_raw;
    entrees->secu_spires  = s_spires_inv     ? !sp_raw : sp_raw;
    entrees->poumon_plein = s_poumon_inv     ? !pp_raw : pp_raw;
    entrees->pression_ok  = s_pressostat_inv ? !ps_raw : ps_raw;
}

// =============================================================================
// Sorties EV bistables
// =============================================================================

void gpio_ev_canon_set(bool actif)
{
    if (actif == s_ev_canon_ouverte) return;
    int pin = actif ? PIN_EV_CANON_OUVRIR : PIN_EV_CANON_FERMER;
    gpio_set_level(pin, 1);
    vTaskDelay(pdMS_TO_TICKS(DUREE_IMPULSION_EV_MS));
    gpio_set_level(pin, 0);
    s_ev_canon_ouverte = actif;
}

void gpio_ev_poumon_set(bool actif)
{
    if (actif == s_ev_poumon_ouverte) return;
    int pin = actif ? PIN_EV_POUMON_OUVRIR : PIN_EV_POUMON_FERMER;
    gpio_set_level(pin, 1);
    vTaskDelay(pdMS_TO_TICKS(DUREE_IMPULSION_EV_MS));
    gpio_set_level(pin, 0);
    s_ev_poumon_ouverte = actif;
}

bool gpio_ev_canon_get(void)
{
    return s_ev_canon_ouverte;
}

bool gpio_ev_poumon_get(void)
{
    return s_ev_poumon_ouverte;
}

void gpio_all_ev_off(void)
{
    // Impulsions FERMER simultanées — 100ms au lieu de 200ms séquentiel
    // Appelable depuis n'importe quel contexte tâche FreeRTOS (pas depuis ISR)
    gpio_set_level(PIN_EV_CANON_FERMER,  1);
    gpio_set_level(PIN_EV_POUMON_FERMER, 1);
    vTaskDelay(pdMS_TO_TICKS(DUREE_IMPULSION_EV_MS));
    gpio_set_level(PIN_EV_CANON_FERMER,  0);
    gpio_set_level(PIN_EV_POUMON_FERMER, 0);
    s_ev_canon_ouverte  = false;
    s_ev_poumon_ouverte = false;
}

// =============================================================================
// Setters runtime
// =============================================================================

void gpio_handler_set_vitesse_depuis_cycles_poumon(bool actif)
{
    portENTER_CRITICAL(&s_mux);
    s_vitesse_depuis_cycles_poumon = actif;
    portEXIT_CRITICAL(&s_mux);
}

void gpio_handler_set_contacts_inv(bool fc, bool spires, bool poumon, bool pressostat)
{
    s_fc_inv         = fc;
    s_spires_inv     = spires;
    s_poumon_inv     = poumon;
    s_pressostat_inv = pressostat;
}

void gpio_handler_set_vitesse_estimee(float vitesse_m_h)
{
    portENTER_CRITICAL(&s_mux);
    s_vitesse_estimee_mh = vitesse_m_h;
    portEXIT_CRITICAL(&s_mux);
}

// =============================================================================
// Vitesse
// =============================================================================

float gpio_get_vitesse_m_h(void)
{
    portENTER_CRITICAL(&s_mux);
    float v = s_vitesse_depuis_cycles_poumon ? s_vitesse_estimee_mh : 0.0f;
    portEXIT_CRITICAL(&s_mux);
    return v;
}

uint32_t gpio_get_impulsions(void)
{
    return s_impulsions;
}

void gpio_reset_impulsions_cycle(void)
{
    portENTER_CRITICAL(&s_mux);
    s_impulsions = 0;
    portEXIT_CRITICAL(&s_mux);
}

// =============================================================================
// Hooks de test
// =============================================================================

#if defined(CONFIG_IRRI_ENABLE_TESTS) || defined(CONFIG_IRRI_TEST_MODE)

void gpio_handler_test_injecter_pulse(int64_t timestamp_us)
{
    (void)timestamp_us;  // signature conservée pour les mocks — seul le comptage subsiste
    portENTER_CRITICAL(&s_mux);
    s_impulsions++;
    portEXIT_CRITICAL(&s_mux);
}

void gpio_handler_test_reset(void)
{
    portENTER_CRITICAL(&s_mux);
    s_impulsions            = 0;
    s_last_isr_us           = 0;
    s_vitesse_depuis_cycles_poumon = false;
    s_vitesse_estimee_mh    = 0.0f;
    s_ev_canon_ouverte      = false;
    s_ev_poumon_ouverte     = false;
    portEXIT_CRITICAL(&s_mux);
}

#endif // CONFIG_IRRI_ENABLE_TESTS || CONFIG_IRRI_TEST_MODE
