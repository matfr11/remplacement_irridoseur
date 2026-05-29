#include "telemetry.h"

// Stub télémétrie — no-op pour les tests PC (pas de WebSocket sur PC)
void telemetry_envoyer_bilan(const session_summary_t *bilan) { (void)bilan; }
esp_err_t telemetry_init(void) { return 0; }
