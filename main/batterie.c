#include "batterie.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"
#include "esp_log.h"

static const char *TAG = "batterie";

static adc_oneshot_unit_handle_t s_adc_handle  = NULL;
static adc_cali_handle_t         s_cali_handle = NULL;
static bool                      s_cali_ok     = false;

static float s_warn_v = BATT_V_FAIBLE_MIN;
static float s_crit_v = BATT_V_CRITIQUE_MIN;

#ifdef CONFIG_IRRI_TEST_MODE
static float s_sim_voltage = 0.0f;
void batterie_sim_set_voltage(float v) { s_sim_voltage = v; }
#endif

esp_err_t batterie_init(void)
{
    adc_oneshot_unit_init_cfg_t unit_cfg = {
        .unit_id  = ADC_UNIT_1,
        .ulp_mode = ADC_ULP_MODE_DISABLE,
    };
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&unit_cfg, &s_adc_handle));

    adc_oneshot_chan_cfg_t chan_cfg = {
        .atten    = ADC_ATTEN_DB_12,
        .bitwidth = ADC_BITWIDTH_12,
    };
    ESP_ERROR_CHECK(adc_oneshot_config_channel(s_adc_handle, BATT_ADC_CHANNEL, &chan_cfg));

#if ADC_CALI_SCHEME_CURVE_FITTING_SUPPORTED
    adc_cali_curve_fitting_config_t cali_cfg = {
        .unit_id  = ADC_UNIT_1,
        .chan     = BATT_ADC_CHANNEL,
        .atten    = ADC_ATTEN_DB_12,
        .bitwidth = ADC_BITWIDTH_12,
    };
    s_cali_ok = (adc_cali_create_scheme_curve_fitting(&cali_cfg, &s_cali_handle) == ESP_OK);
#endif

    if (s_cali_ok)
        ESP_LOGI(TAG, "ADC calibration OK (curve fitting)");
    else
        ESP_LOGW(TAG, "ADC calibration non disponible — mesure approx.");

    ESP_LOGI(TAG, "init OK — GPIO 36, R1=100k R2=27k, max=%.1fV", BATT_V_MAX);
    return ESP_OK;
}

float batterie_lire_voltage(void)
{
#ifdef CONFIG_IRRI_TEST_MODE
    if (s_sim_voltage > 0.0f) return s_sim_voltage;
#endif
    int raw_sum = 0;
    for (int i = 0; i < BATT_ADC_NB_SAMPLES; i++) {
        int raw = 0;
        adc_oneshot_read(s_adc_handle, BATT_ADC_CHANNEL, &raw);
        raw_sum += raw;
    }
    int raw_moy = raw_sum / BATT_ADC_NB_SAMPLES;

    float v_adc;
    if (s_cali_ok) {
        int v_mv = 0;
        adc_cali_raw_to_voltage(s_cali_handle, raw_moy, &v_mv);
        v_adc = v_mv / 1000.0f;
    } else {
        v_adc = (raw_moy / 4095.0f) * 3.3f;
    }

    float v_batt = v_adc * (BATT_R1_KOHM + BATT_R2_KOHM) / BATT_R2_KOHM;

    if (v_batt > BATT_V_MAX * 1.1f) {
        ESP_LOGW(TAG, "Tension aberrante : %.2fV — ignoree", v_batt);
        return 0.0f;
    }
    return v_batt;
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
    else if (s.voltage_v >= BATT_V_CORRECTE_MIN)  s.etat = BATT_ETAT_CORRECTE;
    else if (s.voltage_v >= s_warn_v)             s.etat = BATT_ETAT_FAIBLE;
    else                                          s.etat = BATT_ETAT_CRITIQUE;

    float range = 12.6f - 11.0f;
    float pos   = s.voltage_v - 11.0f;
    s.pourcentage = (int)((pos / range) * 100.0f);
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
