#include "mock_ina3221.h"

static bool             s_ok = false;
static ina3221_mesure_t s_mesures[4]; // index 1..3

esp_err_t ina3221_init(void) { return ESP_OK; }

bool ina3221_est_ok(void) { return s_ok; }

ina3221_mesure_t ina3221_lire_canal(int canal)
{
    if (canal < 1 || canal > 3) return (ina3221_mesure_t){0.0f, 0.0f};
    return s_mesures[canal];
}

float ina3221_lire_tension(int canal)
{
    return ina3221_lire_canal(canal).tension_v;
}

void mock_ina3221_set_ok(bool ok) { s_ok = ok; }

void mock_ina3221_set_canal(int canal, float tension_v, float courant_ma)
{
    if (canal < 1 || canal > 3) return;
    s_mesures[canal] = (ina3221_mesure_t){tension_v, courant_ma};
}

void mock_ina3221_reset(void)
{
    s_ok = false;
    for (int i = 0; i < 4; i++) s_mesures[i] = (ina3221_mesure_t){0.0f, 0.0f};
}
