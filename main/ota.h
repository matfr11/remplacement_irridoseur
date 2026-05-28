#pragma once

#include "esp_err.h"

// =============================================================================
// ota.h — Mise à jour firmware OTA Irrifrance ESP32
// Implémentation complète — PR-08
// =============================================================================

// Démarre le gestionnaire HTTP OTA sur l'URI /ota/update (POST).
// Upload binaire → partition passive → redémarrage automatique.
esp_err_t ota_init(void);
