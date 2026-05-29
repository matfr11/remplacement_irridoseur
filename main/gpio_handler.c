#include "gpio_handler.h"
#include "gpio_config.h"
#include "driver/gpio.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include <string.h>

#ifdef CONFIG_IRRI_TEST_MODE
  #include "simulator/simulator.h"
  #define READ_GPIO(pin)  sim_gpio_get_level(pin)
#else
  #define READ_GPIO(pin)  gpio_get_level(pin)
#endif

static const char *TAG = "gpio_handler";

// --- Fenêtre glissante vitesse ---
#define VITESSE_FENETRE_MAX  20

static portMUX_TYPE s_mux = portMUX_INITIALIZER_UNLOCKED;

static volatile int64_t  s_ts_pulses[VITESSE_FENETRE_MAX];
static volatile int      s_pulse_head  = 0;
static volatile int      s_pulse_count = 0;
static volatile uint32_t s_impulsions  = 0;
static volatile int64_t  s_last_isr_us = 0;
static volatile int      s_cycles_sans_impulsion = 0;

// Paramètres runtime (mis à jour depuis NVS via gpio_handler_set_params)
static int   s_fenetre    = 5;
static int   s_max_cycles = 15;

// Distance par impulsion courante (m) — fournie par calculs_mecanique via state_machine
static float s_dist_pulse_m = 0.0f;

// Mode dégradé A
static bool  s_mode_degrade_a     = false;
static float s_vitesse_estimee_mh = 0.0f;

// =============================================================================
// ISR
// =============================================================================

static void IRAM_ATTR isr_capteur_vitesse(void *arg)
{
    (void)arg;
    int64_t now = esp_timer_get_time();

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

// =============================================================================
// Init
// =============================================================================

void gpio_handler_init(void)
{
    gpio_config_t out_cfg = {
        .pin_bit_mask = (1ULL << PIN_EV_CANON) | (1ULL << PIN_EV_POUMON),
        .mode         = GPIO_MODE_INPUT_OUTPUT,  // INPUT_OUTPUT active le buffer lecture → gpio_get_level() reflète la sortie
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

    ESP_LOGI(TAG, "GPIO init — EV_CANON=%d EV_POUMON=%d", PIN_EV_CANON, PIN_EV_POUMON);
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

// =============================================================================
// Setters runtime
// =============================================================================

void gpio_handler_set_dist_pulse_m(float dist_pulse_m)
{
    portENTER_CRITICAL(&s_mux);
    s_dist_pulse_m = dist_pulse_m;
    portEXIT_CRITICAL(&s_mux);
}

void gpio_handler_set_params(int fenetre_vitesse, int max_cycles_si)
{
    portENTER_CRITICAL(&s_mux);
    s_fenetre    = (fenetre_vitesse > 1 && fenetre_vitesse <= VITESSE_FENETRE_MAX)
                   ? fenetre_vitesse : 5;
    s_max_cycles = (max_cycles_si > 0) ? max_cycles_si : 15;
    portEXIT_CRITICAL(&s_mux);
}

void gpio_handler_set_mode_degrade_a(bool actif)
{
    portENTER_CRITICAL(&s_mux);
    s_mode_degrade_a = actif;
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

float gpio_get_vitesse_m_h(float facteur_correction)
{
    int64_t ts_newest, ts_oldest;
    int     win;

    portENTER_CRITICAL(&s_mux);

    if (s_mode_degrade_a) {
        float v = s_vitesse_estimee_mh;
        portEXIT_CRITICAL(&s_mux);
        return v;
    }

    if (s_cycles_sans_impulsion > s_max_cycles) {
        portEXIT_CRITICAL(&s_mux);
        return 0.0f;
    }

    float dpm = s_dist_pulse_m;
    if (dpm <= 0.0f) {
        portEXIT_CRITICAL(&s_mux);
        return 0.0f;
    }

    int count   = s_pulse_count;
    int head    = s_pulse_head;
    int fenetre = (s_fenetre < count) ? s_fenetre : count;

    if (fenetre < 2) {
        portEXIT_CRITICAL(&s_mux);
        return 0.0f;
    }

    win = fenetre;
    int idx_n = (head - 1       + VITESSE_FENETRE_MAX) % VITESSE_FENETRE_MAX;
    int idx_o = (head - fenetre + VITESSE_FENETRE_MAX) % VITESSE_FENETRE_MAX;
    ts_newest = s_ts_pulses[idx_n];
    ts_oldest = s_ts_pulses[idx_o];

    portEXIT_CRITICAL(&s_mux);

    float dt_s = (float)(ts_newest - ts_oldest) / 1e6f;
    if (dt_s < 1e-3f) {
        return 0.0f;
    }

    // vitesse_m_h = (N × dist_pulse_m × facteur_correction) / dt_s × 3600
    return ((float)win * dpm * facteur_correction / dt_s) * 3600.0f;
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

void gpio_handler_tick_cycle(void)
{
    portENTER_CRITICAL(&s_mux);
    s_cycles_sans_impulsion++;
    portEXIT_CRITICAL(&s_mux);
}

// =============================================================================
// Hooks de test
// =============================================================================

#if defined(CONFIG_IRRI_ENABLE_TESTS) || defined(CONFIG_IRRI_TEST_MODE)

void gpio_handler_test_injecter_pulse(int64_t timestamp_us)
{
    portENTER_CRITICAL(&s_mux);
    s_ts_pulses[s_pulse_head] = timestamp_us;
    s_pulse_head = (s_pulse_head + 1) % VITESSE_FENETRE_MAX;
    if (s_pulse_count < VITESSE_FENETRE_MAX) s_pulse_count++;
    s_impulsions++;
    s_cycles_sans_impulsion = 0;
    portEXIT_CRITICAL(&s_mux);
}

void gpio_handler_test_reset(void)
{
    portENTER_CRITICAL(&s_mux);
    s_pulse_head            = 0;
    s_pulse_count           = 0;
    s_impulsions            = 0;
    s_cycles_sans_impulsion = 0;
    s_dist_pulse_m          = 0.0f;
    s_mode_degrade_a        = false;
    s_vitesse_estimee_mh    = 0.0f;
    portEXIT_CRITICAL(&s_mux);
}

#endif // CONFIG_IRRI_ENABLE_TESTS || CONFIG_IRRI_TEST_MODE
