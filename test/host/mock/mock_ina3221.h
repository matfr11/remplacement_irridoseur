#pragma once

#include "ina3221.h"

// =============================================================================
// mock_ina3221.h — Mock INA3221 contrôlable pour tests host
// Remplace le driver I2C : les tests injectent tension/courant par canal.
// =============================================================================

// Présence du module (défaut false → batterie/mosfet en mode dégradé "sans INA")
void mock_ina3221_set_ok(bool ok);

// Injecte la mesure d'un canal (1=EV_CANON, 2=EV_POUMON, 3=Batterie)
void mock_ina3221_set_canal(int canal, float tension_v, float courant_ma);

// Remet tout à zéro (ok=false, mesures nulles)
void mock_ina3221_reset(void);
