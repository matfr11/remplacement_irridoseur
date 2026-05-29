#include "telemetry.h"

// Stub télémétrie — no-op pour les tests PC (pas de WebSocket sur PC)
void telemetry_envoyer_bilan(void) {}
esp_err_t telemetry_init(void) { return 0; }
