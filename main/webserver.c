#include "webserver.h"
#include "state_machine.h"
#include "config_nvs.h"
#include "webui.h"
#include "esp_log.h"
#include "esp_http_server.h"

static const char *TAG = "webserver";
static httpd_handle_t s_server = NULL;

esp_err_t webserver_start(void)
{
    // TODO PR-07 : configuration HTTP + WebSocket
    // - Handler GET "/" → renvoyer WEBUI_HTML (webui.h)
    // - Handler WebSocket "/ws" → commandes start/stop/reset/set_prog/save_prog/sync_time
    // - Broadcast JSON status depuis ws_broadcast_status()
    ESP_LOGI(TAG, "WebServer démarré (stub — implémentation complète PR-07)");
    return ESP_OK;
}

void webserver_stop(void)
{
    if (s_server) {
        httpd_stop(s_server);
        s_server = NULL;
        ESP_LOGI(TAG, "WebServer arrêté");
    }
}

void ws_broadcast_status(void)
{
    // TODO PR-07 : sérialisation machine_status_t → JSON, envoi à tous les fd WebSocket
    // Format défini dans SPECS.md section 10
    (void)s_server;
}
