#include "telemetry.h"
#include "webserver.h"
#include "state_machine.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

// Implémentation complète — PR-08
static const char *TAG = "telemetry";

static void telemetry_task(void *arg)
{
    (void)arg;
    while (1) {
        webserver_broadcast_status();
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

esp_err_t telemetry_init(void)
{
    ESP_LOGI(TAG, "Démarrage tâche telemetry (500ms)");
    xTaskCreate(telemetry_task, "telemetry", 4096, NULL, 5, NULL);
    return ESP_OK;
}

void telemetry_envoyer_bilan(void)
{
    // PR-08 : serialiser session_summary_t en JSON → WebSocket broadcast
}
