#pragma once

#include "esp_err.h"

// =============================================================================
// telemetry.h — Diffusion statut WebSocket + bilan session Irrifrance ESP32
// =============================================================================

// Démarre la tâche FreeRTOS telemetry (500ms broadcast WebSocket).
esp_err_t telemetry_init(void);

// Envoie le bilan de fin de session (JSON) à tous les clients connectés.
// Reçoit le bilan par pointeur pour éviter de re-prendre le mutex state_machine
// (peut être appelé depuis tick_state_machine qui tient déjà le mutex).
#include "state_machine.h"
void telemetry_envoyer_bilan(const session_summary_t *bilan);
