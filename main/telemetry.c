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
    xTaskCreate(telemetry_task, "telemetry", 6144, NULL, 5, NULL);
    return ESP_OK;
}

void telemetry_envoyer_bilan(void)
{
    session_summary_t b;
    state_machine_get_session_summary(&b);
    char json[256];
    snprintf(json, sizeof(json),
        "{\"type\":\"bilan\","
        "\"longueur_m\":%.1f,"
        "\"surface_m2\":%.1f,"
        "\"dose_moy_mm\":%.2f,"
        "\"volume_m3\":%.2f,"
        "\"duree_s\":%d,"
        "\"nb_cycles\":%u,"
        "\"duree_pause_s\":%d}",
        b.longueur_m,
        b.surface_m2,
        b.dose_moy_mm,
        b.volume_m3,
        (int)b.duree_s,
        (unsigned int)b.nb_cycles,
        (int)b.duree_pause_pression_s);
    webserver_broadcast_raw(json);
    ESP_LOGI("telemetry", "Bilan session envoyé : %.1fm / %.1fm2 / %ds",
             b.longueur_m, b.surface_m2, (int)b.duree_s);
}
