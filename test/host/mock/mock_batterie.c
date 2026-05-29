#include "batterie.h"
#include "esp_err.h"

esp_err_t batterie_init(void) { return ESP_OK; }
float batterie_lire_voltage(void) { return 12.5f; }
void batterie_set_seuils(float w, float c) { (void)w; (void)c; }

batt_status_t batterie_get_status(void)
{
    batt_status_t s;
    s.voltage_v  = 12.5f;
    s.etat       = BATT_ETAT_PLEINE;
    s.pourcentage = 94;
    return s;
}

const char* batterie_etat_str(batt_etat_t e)    { (void)e; return "Pleine"; }
const char* batterie_etat_couleur(batt_etat_t e) { (void)e; return "#22c55e"; }
