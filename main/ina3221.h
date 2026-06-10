#pragma once

#include "esp_err.h"
#include <stdbool.h>

// =============================================================================
// ina3221.h — Driver INA3221 3 canaux I2C
// CH1 = EV_CANON, CH2 = EV_POUMON, CH3 = Batterie
// Shunts intégrés 0.1Ω (R100) sur MCU-3221
// =============================================================================

typedef struct {
    float tension_v;    // Tension bus (V)
    float courant_ma;   // Courant (mA)
} ina3221_mesure_t;

// Initialise le bus I2C et configure l'INA3221.
esp_err_t ina3221_init(void);

// Lit tension et courant d'un canal (1, 2 ou 3).
ina3221_mesure_t ina3221_lire_canal(int canal);

// Lecture tension uniquement (plus rapide — un seul registre).
float ina3221_lire_tension(int canal);

// Retourne true si le module est initialisé et opérationnel.
bool ina3221_est_ok(void);
