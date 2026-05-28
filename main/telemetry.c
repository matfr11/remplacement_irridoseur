#include "telemetry.h"
#include "esp_log.h"

static const char *TAG = "telemetry";

// Phase 1 : implémentations vides — log série uniquement.
// Phase 2 : brancher module LoRa SX1276/SX1262 sur SPI.
//   GPIOs SPI disponibles : MOSI=23, MISO=19, SCK=18, CS=5, RST=14, DIO0=2

void telemetry_send_status(const machine_status_t *status)
{
    // Phase 1 : rien (le WebSocket remplace côté terrain)
    (void)status;
}

void telemetry_send_alarm(alarm_type_t alarm, const char *message)
{
    // Phase 1 : log uniquement
    ESP_LOGW(TAG, "ALARM [%d] : %s", (int)alarm, message);
}

void telemetry_send_session_end(const session_summary_t *summary)
{
    if (!summary) return;
    ESP_LOGI(TAG,
             "Bilan session — surface=%.0f m²  dose=%.1f mm  durée=%lu s  volume=%.1f m³",
             summary->surface_m2, summary->dose_moy_mm,
             (unsigned long)summary->duree_s, summary->volume_m3);
}
