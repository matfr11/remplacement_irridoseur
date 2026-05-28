#include "gpio_handler.h"
#include "driver/gpio.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"

static const char *TAG = "gpio";

// Variable globale capteur vitesse — accessible depuis ISR (IRAM) et tâche
capteur_vitesse_t g_capteur_vitesse = {0};

// Sens des contacteurs — chargé depuis NVS via gpio_handler_set_sens()
// Défaut : tous NO (actif bas), cohérent avec schéma câblage pull-up 10kΩ
static gpio_sens_t s_sens = {
    .contact_no_fin_course      = true,
    .contact_no_securite_spires = true,
    .contact_no_poumon          = true,
    .contact_no_pressostat      = true,
};

// Lecture d'un contact en tenant compte de son type NO/NC
static inline bool lire_contact(gpio_num_t pin, bool contact_no)
{
    // NO (contact_no=true)  : actif quand GPIO LOW  → !gpio_get_level()
    // NC (contact_no=false) : actif quand GPIO HIGH →  gpio_get_level()
    int level = gpio_get_level(pin);
    return contact_no ? !level : level;
}

// ISR capteur vitesse — exécutée depuis IRAM, latence minimale
// Front montant GPIO 34 (diviseur 10kΩ/3.3kΩ : 0V → ~3V)
static void IRAM_ATTR isr_vitesse(void *arg)
{
    uint64_t now_us   = esp_timer_get_time();
    uint64_t delta_us = now_us - g_capteur_vitesse.last_isr_us;

    // Anti-rebond logiciel : rejeter les fronts trop proches
    if (delta_us < DEBOUNCE_VITESSE_US) {
        return;
    }

    g_capteur_vitesse.periode_us    = (uint32_t)delta_us;
    g_capteur_vitesse.last_isr_us   = now_us;
    g_capteur_vitesse.nb_impulsions++;
}

void gpio_handler_init(void)
{
    // Entrées contacts secs — pull-up 10kΩ externe câblé (pull-up interne désactivé)
    gpio_config_t cfg_in = {
        .pin_bit_mask = (1ULL << PIN_FIN_COURSE)      |
                        (1ULL << PIN_SECURITE_SPIRES)  |
                        (1ULL << PIN_CONTACT_POUMON)   |
                        (1ULL << PIN_PRESSOSTAT),
        .mode         = GPIO_MODE_INPUT,
        .pull_up_en   = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type    = GPIO_INTR_DISABLE,
    };
    ESP_ERROR_CHECK(gpio_config(&cfg_in));

    // GPIO 34 : capteur vitesse inductif NPN 12V — input only, pas de pull-up interne
    // Diviseur 10kΩ/3.3kΩ ramène 12V → ~3V compatible ESP32
    gpio_config_t cfg_vitesse = {
        .pin_bit_mask = (1ULL << PIN_CAPTEUR_VITESSE),
        .mode         = GPIO_MODE_INPUT,
        .pull_up_en   = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type    = GPIO_INTR_POSEDGE,  // Front montant 0V → 3V
    };
    ESP_ERROR_CHECK(gpio_config(&cfg_vitesse));

    // Sorties relais — actif bas (LOW = relais actif = électrovanne ouverte)
    gpio_config_t cfg_out = {
        .pin_bit_mask = (1ULL << PIN_EV1) | (1ULL << PIN_EV2),
        .mode         = GPIO_MODE_OUTPUT,
        .pull_up_en   = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type    = GPIO_INTR_DISABLE,
    };
    ESP_ERROR_CHECK(gpio_config(&cfg_out));

    // État initial sûr : électrovannes fermées
    gpio_all_ev_off();

    // ISR capteur vitesse
    ESP_ERROR_CHECK(gpio_install_isr_service(0));
    ESP_ERROR_CHECK(gpio_isr_handler_add(PIN_CAPTEUR_VITESSE, isr_vitesse, NULL));

    ESP_LOGI(TAG, "GPIO initialisés — EV1=OFF EV2=OFF ISR vitesse active");
    ESP_LOGI(TAG, "Sens contacteurs par défaut : NO (actif bas) — à vérifier terrain");
}

void gpio_handler_set_sens(const gpio_sens_t *sens)
{
    s_sens = *sens;
    ESP_LOGI(TAG, "Sens contacteurs mis à jour : fin_crs=%s secu=%s poumon=%s press=%s",
             sens->contact_no_fin_course      ? "NO" : "NC",
             sens->contact_no_securite_spires ? "NO" : "NC",
             sens->contact_no_poumon          ? "NO" : "NC",
             sens->contact_no_pressostat      ? "NO" : "NC");
}

void gpio_handler_lire_entrees(entrees_t *entrees)
{
    entrees->fin_course      = lire_contact(PIN_FIN_COURSE,      s_sens.contact_no_fin_course);
    entrees->securite_spires = lire_contact(PIN_SECURITE_SPIRES, s_sens.contact_no_securite_spires);
    entrees->contact_poumon  = lire_contact(PIN_CONTACT_POUMON,  s_sens.contact_no_poumon);
    entrees->pressostat      = lire_contact(PIN_PRESSOSTAT,      s_sens.contact_no_pressostat);
}

void gpio_ev1_set(bool actif)
{
    gpio_set_level(PIN_EV1, actif ? 0 : 1);
}

void gpio_ev2_set(bool actif)
{
    gpio_set_level(PIN_EV2, actif ? 0 : 1);
}

void gpio_all_ev_off(void)
{
    // Fail-safe : HIGH = relais repos = électrovannes fermées
    gpio_set_level(PIN_EV1, 1);
    gpio_set_level(PIN_EV2, 1);
}
