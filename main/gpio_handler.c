#include "gpio_handler.h"
#include "gpio_config.h"
#include "driver/gpio.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include <string.h>

// Implémentation complète — PR-02
static const char *TAG = "gpio_handler";

// --- Vitesse : fenêtre glissante sur N impulsions ---
#define VITESSE_FENETRE_MAX  20

static portMUX_TYPE s_mux = portMUX_INITIALIZER_UNLOCKED;

static volatile int64_t  s_ts_pulses[VITESSE_FENETRE_MAX];  // timestamps µs
static volatile int      s_pulse_head  = 0;
static volatile int      s_pulse_count = 0;
static volatile uint32_t s_impulsions  = 0;   // compteur session
static volatile int64_t  s_last_isr_us = 0;
static volatile int      s_cycles_sans_impulsion = 0;

// Paramètres runtime (mis à jour depuis config NVS)
static int   s_fenetre      = 5;
static int   s_max_cycles   = 15;

static void IRAM_ATTR isr_capteur_vitesse(void *arg)
{
    (void)arg;
    int64_t now = esp_timer_get_time();

    // Anti-rebond temporel
    if (now - s_last_isr_us < (int64_t)DEBOUNCE_VITESSE_MS * 1000) {
        return;
    }
    s_last_isr_us = now;

    portENTER_CRITICAL_ISR(&s_mux);
    s_ts_pulses[s_pulse_head] = now;
    s_pulse_head = (s_pulse_head + 1) % VITESSE_FENETRE_MAX;
    if (s_pulse_count < VITESSE_FENETRE_MAX) s_pulse_count++;
    s_impulsions++;
    s_cycles_sans_impulsion = 0;
    portEXIT_CRITICAL_ISR(&s_mux);
}

void gpio_handler_init(void)
{
    // Sorties électrovannes
    gpio_config_t out_cfg = {
        .pin_bit_mask = (1ULL << PIN_EV_CANON) | (1ULL << PIN_EV_POUMON),
        .mode         = GPIO_MODE_OUTPUT,
        .pull_up_en   = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type    = GPIO_INTR_DISABLE,
    };
    gpio_config(&out_cfg);
    gpio_set_level(PIN_EV_CANON,  0);
    gpio_set_level(PIN_EV_POUMON, 0);

    // Entrées capteurs (pas de pull interne — résistances externes sur câblage)
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

    // ISR capteur vitesse — RISING uniquement
    gpio_set_intr_type(PIN_CAPTEUR_VITESSE, GPIO_INTR_POSEDGE);
    gpio_install_isr_service(0);
    gpio_isr_handler_add(PIN_CAPTEUR_VITESSE, isr_capteur_vitesse, NULL);

    ESP_LOGI(TAG, "GPIO initialisé — EV_CANON=%d EV_POUMON=%d", PIN_EV_CANON, PIN_EV_POUMON);
}

void gpio_handler_lire_entrees(entrees_t *entrees)
{
    // Contacts NC : HIGH = activé/danger
    entrees->fin_course   = gpio_get_level(PIN_FIN_COURSE)   != 0;
    entrees->secu_spires  = gpio_get_level(PIN_SECU_SPIRES)  != 0;
    entrees->poumon_plein = gpio_get_level(PIN_POUMON_PLEIN) != 0;
    entrees->pression_ok  = gpio_get_level(PIN_PRESSOSTAT)   == 0;  // LOW = pression présente
}

void gpio_ev_canon_set(bool actif)
{
    gpio_set_level(PIN_EV_CANON, actif ? 1 : 0);
}

void gpio_ev_poumon_set(bool actif)
{
    gpio_set_level(PIN_EV_POUMON, actif ? 1 : 0);
}

void gpio_all_ev_off(void)
{
    gpio_set_level(PIN_EV_CANON,  0);
    gpio_set_level(PIN_EV_POUMON, 0);
}

float gpio_get_vitesse_m_h(float facteur_correction)
{
    portENTER_CRITICAL(&s_mux);
    int    count = s_pulse_count;
    int    head  = s_pulse_head;
    int    fenetre = (s_fenetre < count) ? s_fenetre : count;
    int64_t ts_newest, ts_oldest;
    portEXIT_CRITICAL(&s_mux);

    if (fenetre < 2) {
        return 0.0f;
    }
    if (s_cycles_sans_impulsion > s_max_cycles) {
        return 0.0f;
    }

    portENTER_CRITICAL(&s_mux);
    int idx_newest = (head - 1 + VITESSE_FENETRE_MAX) % VITESSE_FENETRE_MAX;
    int idx_oldest = (head - fenetre + VITESSE_FENETRE_MAX) % VITESSE_FENETRE_MAX;
    ts_newest = s_ts_pulses[idx_newest];
    ts_oldest = s_ts_pulses[idx_oldest];
    portEXIT_CRITICAL(&s_mux);

    float dt_s = (float)(ts_newest - ts_oldest) / 1e6f;
    if (dt_s < 1e-3f) {
        return 0.0f;
    }

    // dist_pulse_m non connu ici — fourni à l'appel par state_machine via facteur_correction
    // Pour PR-01 skeleton, retourne 0 — PR-02 câble dist_pulse depuis calculs_mecanique
    (void)facteur_correction;
    return 0.0f;
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

// Appelé depuis tick_state_machine à chaque cycle poumon (100ms)
// pour incrémenter le compteur de cycles sans impulsion
void gpio_handler_tick_cycle(void)
{
    portENTER_CRITICAL(&s_mux);
    s_cycles_sans_impulsion++;
    portEXIT_CRITICAL(&s_mux);
}
