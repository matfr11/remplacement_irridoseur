#pragma once

#include "esp_err.h"

// =============================================================================
// telemetry.h — Diffusion statut WebSocket + bilan session Irrifrance ESP32
// =============================================================================

// Démarre la tâche FreeRTOS telemetry (500ms broadcast WebSocket).
esp_err_t telemetry_init(void);

// Envoie le bilan de fin de session (JSON) à tous les clients connectés.
void telemetry_envoyer_bilan(void);
