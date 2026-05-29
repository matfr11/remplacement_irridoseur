#pragma once

#include "esp_err.h"

// =============================================================================
// webserver.h — WiFi AP + HTTP + WebSocket Irrifrance ESP32
// Implémentation complète — PR-08 / PR-09
// =============================================================================

// Démarre le point d'accès WiFi et le serveur HTTP/WebSocket.
// SSID : "IRRIDOSEUR-XXXX" (4 derniers octets MAC)
// Web UI servie depuis webui.h (fichiers embarqués)
esp_err_t webserver_init(void);

// Diffuse le statut machine (machine_status_t) en JSON à tous les clients WS.
// Appelé depuis la tâche telemetry toutes les 500ms.
void webserver_broadcast_status(void);

// Diffuse un JSON arbitraire (déjà sérialisé) à tous les clients WS.
// Utilisé par telemetry_envoyer_bilan().
void webserver_broadcast_raw(const char *json);

// Arrête proprement le serveur et le WiFi.
void webserver_stop(void);
