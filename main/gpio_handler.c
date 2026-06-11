#include "gpio_handler.h"
#include "gpio_config.h"
#include "mosfet_surveillance.h"
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
    // Sorties EV principales uniquement — OUT3/OUT4 et relais sont sous
    // responsabilité exclusive de mosfet_surveillance_init()
    gpio_config_t out_cfg = {
        .pin_bit_mask = (1ULL << PIN_EV_CANON) |
                        (1ULL << PIN_EV_POUMON),
        .mode         = GPIO_MODE_INPUT_OUTPUT,
        .pull_up_en   = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type    = GPIO_INTR_DISABLE,
    };
    gpio_config(&out_cfg);
    gpio_set_level(PIN_EV_CANON,  0);
    gpio_set_level(PIN_EV_POUMON, 0);

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

    gpio_set_intr_type(PIN_CAPTEUR_VITESSE, GPIO_INTR_POSEDGE);
    gpio_install_isr_service(0);
    gpio_isr_handler_add(PIN_CAPTEUR_VITESSE, isr_capteur_vitesse, NULL);

    ESP_LOGI(TAG, "GPIO init — EV_CANON=%d EV_POUMON=%d secours=%d/%d",
             PIN_EV_CANON, PIN_EV_POUMON,
             PIN_MOSFET_SECOURS_CANON, PIN_MOSFET_SECOURS_POUMON);
}

// =============================================================================
// Entrées
// =============================================================================

void gpio_handler_lire_entrees(entrees_t *entrees)
{
    entrees->fin_course   = READ_GPIO(PIN_FIN_COURSE)   != 0;
    entrees->secu_spires  = READ_GPIO(PIN_SECU_SPIRES)  != 0;
    entrees->poumon_plein = READ_GPIO(PIN_POUMON_PLEIN) != 0;
    entrees->pression_ok  = READ_GPIO(PIN_PRESSOSTAT)   == 0;
}

// =============================================================================
// Sorties
// =============================================================================

void gpio_ev_canon_set(bool actif)
{
    // Lire depuis le pin actif AVANT verifier_avant (peut être le secours)
    int pin_reel = mosfet_secours_actif(PIN_EV_CANON)
                 ? PIN_MOSFET_SECOURS_CANON : PIN_EV_CANON;
    bool etat_actuel = gpio_get_level(pin_reel) != 0;
    mosfet_verifier_avant(PIN_EV_CANON, etat_actuel);

    // Recalculer : verifier_avant peut avoir déclenché un basculement
    pin_reel = mosfet_secours_actif(PIN_EV_CANON)
             ? PIN_MOSFET_SECOURS_CANON : PIN_EV_CANON;
    gpio_set_level(pin_reel, actif ? 1 : 0);
    // verifier_apres déplacé dans mosfet_verifier_post_tick() — hors mutex
}

void gpio_ev_poumon_set(bool actif)
{
    // Lire depuis le pin actif AVANT verifier_avant (peut être le secours)
    int pin_reel = mosfet_secours_actif(PIN_EV_POUMON)
                 ? PIN_MOSFET_SECOURS_POUMON : PIN_EV_POUMON;
    bool etat_actuel = gpio_get_level(pin_reel) != 0;
    mosfet_verifier_avant(PIN_EV_POUMON, etat_actuel);

    // Recalculer : verifier_avant peut avoir déclenché un basculement
    pin_reel = mosfet_secours_actif(PIN_EV_POUMON)
             ? PIN_MOSFET_SECOURS_POUMON : PIN_EV_POUMON;
    gpio_set_level(pin_reel, actif ? 1 : 0);
    // verifier_apres déplacé dans mosfet_verifier_post_tick() — hors mutex
}

bool gpio_ev_canon_get(void)
{
    int pin = mosfet_secours_actif(PIN_EV_CANON)
            ? PIN_MOSFET_SECOURS_CANON : PIN_EV_CANON;
    return gpio_get_level(pin) != 0;
}

bool gpio_ev_poumon_get(void)
{
    int pin = mosfet_secours_actif(PIN_EV_POUMON)
            ? PIN_MOSFET_SECOURS_POUMON : PIN_EV_POUMON;
    return gpio_get_level(pin) != 0;
}

void gpio_all_ev_off(void)
{
    // Arrêt d'urgence — DIRECT et INCONDITIONNEL, sans surveillance MOSFET.
    // Coupe principal ET secours ET remet les relais en position repos (NC).
    gpio_set_level(PIN_EV_CANON,              0);
    gpio_set_level(PIN_EV_POUMON,             0);
    gpio_set_level(PIN_MOSFET_SECOURS_CANON,  0);
    gpio_set_level(PIN_MOSFET_SECOURS_POUMON, 0);
    gpio_set_level(PIN_RELAIS_CANON,          0);
    gpio_set_level(PIN_RELAIS_POUMON,         0);
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
    portEXIT_CRITICAL(&s_mux);
}

#endif // CONFIG_IRRI_ENABLE_TESTS || CONFIG_IRRI_TEST_MODE
