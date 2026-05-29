#pragma once

#include "esp_err.h"
#include "esp_http_server.h"

// =============================================================================
// ota.h — Mise à jour firmware OTA Irrifrance ESP32
// =============================================================================

esp_err_t ota_init(void);

// Enregistre le handler POST /ota/update sur le serveur httpd existant.
// Appelé depuis webserver_init() après httpd_start().
void ota_register_handler(httpd_handle_t server);
