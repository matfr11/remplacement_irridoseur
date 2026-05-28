#pragma once

#include "esp_err.h"

// Démarrage du serveur HTTP avec support WebSocket
// Enregistre les handlers URI : GET / (web UI), WS /ws (WebSocket)
esp_err_t webserver_start(void);

// Arrêt propre du serveur HTTP
void webserver_stop(void);

// Sérialise machine_status_t en JSON et diffuse à tous les clients WebSocket connectés
// Appelée depuis websocket_task() toutes les 500ms
void ws_broadcast_status(void);
