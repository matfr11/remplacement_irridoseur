#include "batterie.h"
#include "ina3221.h"
#include "gpio_config.h"
#include "esp_log.h"

static const char *TAG = "batterie";

static float s_warn_v      = BATT_V_FAIBLE_MIN;
static float s_crit_v      = BATT_V_CRITIQUE_MIN;
static float s_last_valid_v = 12.5f;

#ifdef CONFIG_IRRI_TEST_MODE
static float s_sim_voltage = 0.0f;
void batterie_sim_set_voltage(float v) { s_sim_voltage = v; }
#endif

esp_err_t batterie_init(void)
{
    // INA3221 déjà initialisé par ina3221_init() dans app_main().
    ESP_LOGI(TAG, "init OK — tension batterie via INA3221 CH%d", INA3221_CH_BATTERIE);
    return ESP_OK;
}

float batterie_lire_voltage(void)
{
#ifdef CONFIG_IRRI_TEST_MODE
    if (s_sim_voltage > 0.0f) return s_sim_voltage;
#endif

    float v = ina3221_lire_tension(INA3221_CH_BATTERIE);

    if (v > BATT_V_MAX * 1.1f) {
        ESP_LOGW(TAG, "Tension aberrante %.2fV — retour %.2fV", v, s_last_valid_v);
        return s_last_valid_v;
    }
    if (v < 0.5f) {
        ESP_LOGW(TAG, "Lecture invalide %.2fV — retour %.2fV", v, s_last_valid_v);
        return s_last_valid_v;
    }
    s_last_valid_v = v;
    return v;
}

void batterie_set_seuils(float warn_v, float crit_v)
{
    s_warn_v = warn_v;
    s_crit_v = crit_v;
}

batt_status_t batterie_get_status(void)
{
    batt_status_t s;
    s.voltage_v = batterie_lire_voltage();

    if      (s.voltage_v >= BATT_V_CHARGE_MIN)   s.etat = BATT_ETAT_CHARGE;
    else if (s.voltage_v >= BATT_V_PLEINE_MIN)    s.etat = BATT_ETAT_PLEINE;
    else if (s.voltage_v >= s_warn_v)             s.etat = BATT_ETAT_CORRECTE;
    else if (s.voltage_v >= s_crit_v)             s.etat = BATT_ETAT_FAIBLE;
    else                                          s.etat = BATT_ETAT_CRITIQUE;

    // 0 % au seuil critique configurable (cohérent avec le passage en CRITIQUE),
    // 100 % à BATT_V_PCT_100 (plomb chargé au repos)
    float range = BATT_V_PCT_100 - s_crit_v;
    float pos   = s.voltage_v - s_crit_v;
    s.pourcentage = (range > 0.0f) ? (int)((pos / range) * 100.0f) : 0;
    if (s.pourcentage > 100) s.pourcentage = 100;
    if (s.pourcentage < 0)   s.pourcentage = 0;

    return s;
}

const char* batterie_etat_str(batt_etat_t etat)
{
    switch (etat) {
        case BATT_ETAT_CHARGE:   return "En charge";
        case BATT_ETAT_PLEINE:   return "Pleine";
        case BATT_ETAT_CORRECTE: return "Correcte";
        case BATT_ETAT_FAIBLE:   return "Faible";
        case BATT_ETAT_CRITIQUE: return "Critique";
        default:                 return "Inconnue";
    }
}

const char* batterie_etat_couleur(batt_etat_t etat)
{
    switch (etat) {
        case BATT_ETAT_CHARGE:   return "#3b82f6";
        case BATT_ETAT_PLEINE:   return "#22c55e";
        case BATT_ETAT_CORRECTE: return "#eab308";
        case BATT_ETAT_FAIBLE:   return "#f97316";
        case BATT_ETAT_CRITIQUE: return "#ef4444";
        default:                 return "#6b7280";
    }
}
