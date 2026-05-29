#include "webserver.h"
#include "webui.h"
#include "ota.h"
#include "state_machine.h"
#include "config_nvs.h"
#include "esp_wifi.h"
#include "esp_netif.h"
#include "esp_event.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include <string.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>

static const char *TAG = "webserver";

#define MAX_WS_CLIENTS  4
#define JSON_BUF_SIZE   1500

static httpd_handle_t s_server = NULL;

// =============================================================================
// WiFi AP
// =============================================================================

static void wifi_ap_init(void)
{
    esp_netif_init();
    esp_event_loop_create_default();
    esp_netif_create_default_wifi_ap();

    wifi_init_config_t init_cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&init_cfg);

    wifi_config_t ap_cfg = {
        .ap = {
            .ssid           = "Irrifrance-ESP32",
            .password       = "irrigation",
            .ssid_len       = 0,
            .max_connection = MAX_WS_CLIENTS,
            .authmode       = WIFI_AUTH_WPA2_PSK,
            .channel        = 6,
        },
    };

    esp_wifi_set_mode(WIFI_MODE_AP);
    esp_wifi_set_config(WIFI_IF_AP, &ap_cfg);
    esp_wifi_start();

    ESP_LOGI(TAG, "WiFi AP demarre — SSID: Irrifrance-ESP32  IP: 192.168.4.1");
}

// =============================================================================
// Sérialisation JSON statut
// =============================================================================

static const char *etat_to_str(etat_machine_t etat)
{
    switch (etat) {
        case ETAT_VEILLE:             return "VEILLE";
        case ETAT_OUVERTURE_CANON:    return "OUVERTURE_CANON";
        case ETAT_TEMPO_DEPART:       return "TEMPO_DEPART";
        case ETAT_REMPLISSAGE_POUMON: return "REMPLISSAGE_POUMON";
        case ETAT_EN_COURS:           return "EN_COURS";
        case ETAT_PAUSE_PRESSION:     return "PAUSE_PRESSION";
        case ETAT_TEMPO_ARRIVEE:      return "TEMPO_ARRIVEE";
        case ETAT_ARRET_FINAL:        return "ARRET_FINAL";
        case ETAT_ARRET_URGENCE:      return "ARRET_URGENCE";
        default:                      return "INCONNU";
    }
}

static int status_to_json(const machine_status_t *s, char *buf, size_t len)
{
    return snprintf(buf, len,
        "{"
        "\"etat\":\"%s\",\"etat_code\":%d,"
        "\"prog_nom\":\"%s\","
        "\"machine_nom\":\"%s\","
        "\"abaque_nom\":\"%s\","
        "\"longueur_deroulee_m\":%.1f,"
        "\"longueur_enroulee_m\":%.1f,"
        "\"vitesse_m_h\":%.2f,\"vitesse_cible_m_h\":%.2f,"
        "\"duree_s\":%d,"
        "\"heure_arrivee_unix\":%lld,"
        "\"heure_arrivee_relative_s\":%d,"
        "\"heure_synchro\":%s,"
        "\"surface_m2\":%.1f,"
        "\"dose_inst_mm\":%.2f,\"debit_m3h\":%.2f,"
        "\"p_enrouleur_bar\":%.2f,\"p_buse_bar\":%.2f,"
        "\"etage_courant\":%d,\"nb_etages\":%d,"
        "\"ev_canon\":%s,\"ev_poumon\":%s,"
        "\"pression_ok\":%s,"
        "\"fin_course\":%s,"
        "\"secu_spires\":%s,"
        "\"poumon_plein\":%s,"
        "\"t_remplissage_ms\":%d,\"t_attente_ms\":%d,"
        "\"dist_par_cycle_m\":%.4f,"
        "\"cycles_par_min_cible\":%.2f,"
        "\"cycles_par_min_reel\":%.2f,"
        "\"cycles_total\":%u,"
        "\"alerte_pression_insuff\":%s,"
        "\"mode_degrade_vitesse\":%s,"
        "\"mode_degrade_poumon\":%s,"
        "\"facteur_correction\":%.4f,"
        "\"raison_arret\":\"%s\","
        "\"cfg_valide\":%s"
        "}",
        etat_to_str(s->etat), (int)s->etat,
        s->prog_nom,
        s->machine_nom,
        s->abaque_nom,
        s->longueur_deroulee_m,
        s->longueur_enroulee_m,
        s->vitesse_m_h, s->vitesse_cible_m_h,
        (int)s->duree_s,
        (long long)s->heure_arrivee_unix,
        (int)s->heure_arrivee_relative_s,
        s->heure_synchro        ? "true" : "false",
        s->surface_m2,
        s->dose_inst_mm, s->debit_m3h,
        s->p_enrouleur_bar, s->p_buse_bar,
        s->etage_courant, s->nb_etages,
        s->ev_canon             ? "true" : "false",
        s->ev_poumon            ? "true" : "false",
        s->pression_ok          ? "true" : "false",
        s->fin_course           ? "true" : "false",
        s->secu_spires          ? "true" : "false",
        s->poumon_plein         ? "true" : "false",
        (int)s->t_remplissage_ms, (int)s->t_attente_ms,
        s->dist_par_cycle_m,
        s->cycles_par_min_cible,
        s->cycles_par_min_reel,
        (unsigned int)s->cycles_total,
        s->alerte_pression_insuff ? "true" : "false",
        s->mode_degrade_vitesse   ? "true" : "false",
        s->mode_degrade_poumon    ? "true" : "false",
        s->facteur_correction,
        s->raison_arret,
        s->cfg_valide           ? "true" : "false"
    );
}

// =============================================================================
// Parsing commandes WebSocket (JSON minimal sans bibliothèque)
// =============================================================================

static bool json_parse_float(const char *data, const char *key, float *out)
{
    char search[48];
    snprintf(search, sizeof(search), "\"%s\":", key);
    const char *p = strstr(data, search);
    if (!p) return false;
    p += strlen(search);
    while (*p == ' ') p++;
    return sscanf(p, "%f", out) == 1;
}

static bool json_parse_int(const char *data, const char *key, int *out)
{
    char search[48];
    snprintf(search, sizeof(search), "\"%s\":", key);
    const char *p = strstr(data, search);
    if (!p) return false;
    p += strlen(search);
    while (*p == ' ') p++;
    return sscanf(p, "%d", out) == 1;
}

static bool json_parse_bool(const char *data, const char *key, bool *out)
{
    char search[48];
    snprintf(search, sizeof(search), "\"%s\":", key);
    const char *p = strstr(data, search);
    if (!p) return false;
    p += strlen(search);
    while (*p == ' ') p++;
    if (strncmp(p, "true",  4) == 0) { *out = true;  return true; }
    if (strncmp(p, "false", 5) == 0) { *out = false; return true; }
    return false;
}

static void json_parse_string(const char *data, const char *key, char *out, size_t out_len)
{
    char search[48];
    snprintf(search, sizeof(search), "\"%s\":\"", key);
    const char *p = strstr(data, search);
    if (!p) return;
    p += strlen(search);
    size_t i = 0;
    while (*p && *p != '"' && i < out_len - 1) out[i++] = *p++;
    out[i] = '\0';
}

static void handle_ws_command(const char *data, size_t len)
{
    (void)len;

    if (strstr(data, "\"cmd\":\"start\"")) {
        state_machine_cmd_start();

    } else if (strstr(data, "\"cmd\":\"stop\"")) {
        state_machine_cmd_stop();

    } else if (strstr(data, "\"cmd\":\"reset\"")) {
        state_machine_cmd_reset();

    } else if (strstr(data, "\"cmd\":\"set_time\"")) {
        const char *p = strstr(data, "\"ts\":");
        if (p) {
            long long ts = 0;
            sscanf(p + 5, "%lld", &ts);
            state_machine_set_time((int64_t)ts);
        }

    } else if (strstr(data, "\"cmd\":\"ev_canon\"")) {
        bool actif = false;
        json_parse_bool(data, "actif", &actif);
        state_machine_cmd_ev_canon_set(actif);

    } else if (strstr(data, "\"cmd\":\"ev_poumon\"")) {
        bool actif = false;
        json_parse_bool(data, "actif", &actif);
        state_machine_cmd_ev_poumon_set(actif);

    } else if (strstr(data, "\"cmd\":\"select_programme\"")) {
        int idx = 0;
        json_parse_int(data, "idx", &idx);
        config_nvs_sauver_prog_actif(idx);

    } else if (strstr(data, "\"cmd\":\"etalonner\"")) {
        float longueur_m = 0.0f;
        json_parse_float(data, "longueur_m", &longueur_m);
        state_machine_cmd_etalonner(longueur_m);

    } else if (strstr(data, "\"cmd\":\"save_programme\"")) {
        config_programme_t prog;
        memset(&prog, 0, sizeof(prog));
        int idx = 0;
        float f; int n; bool b;
        json_parse_int   (data, "idx",            &idx);
        json_parse_string(data, "nom",             prog.nom, sizeof(prog.nom));
        if (json_parse_float(data, "dose_mm",      &f)) prog.dose_mm       = f;
        if (json_parse_float(data, "largeur_m",    &f)) prog.largeur_m     = f;
        if (json_parse_int  (data, "buse_mm",      &n)) prog.buse_mm       = n;
        if (json_parse_float(data, "pression_bar", &f)) prog.pression_bar  = f;
        if (json_parse_bool (data, "tempo_depart_on",  &b)) prog.tempo_depart_on  = b;
        if (json_parse_int  (data, "tempo_depart_s",   &n)) prog.tempo_depart_s   = n;
        if (json_parse_bool (data, "tempo_arrivee_on", &b)) prog.tempo_arrivee_on = b;
        if (json_parse_int  (data, "tempo_arrivee_s",  &n)) prog.tempo_arrivee_s  = n;
        config_nvs_sauver_programme(idx, &prog);

    } else if (strstr(data, "\"cmd\":\"save_machine\"")) {
        config_machine_t cfg;
        config_nvs_lire_machine(&cfg);
        float f; int n; bool b;
        if (json_parse_float(data, "facteur_correction", &f)) cfg.facteur_correction = f;
        if (json_parse_float(data, "kp_regulation",      &f)) cfg.kp_regulation      = f;
        if (json_parse_int  (data, "n_cycles_calib",     &n)) cfg.n_cycles_calib     = n;
        if (json_parse_int  (data, "fenetre_vitesse",    &n)) cfg.fenetre_vitesse     = n;
        if (json_parse_int  (data, "max_cycles_si",      &n)) cfg.max_cycles_si       = n;
        if (json_parse_float(data, "t_vidange_s",        &f)) cfg.t_vidange_s         = f;
        if (json_parse_float(data, "t_rempl_fixe_s",     &f)) cfg.t_rempl_fixe_s      = f;
        if (json_parse_float(data, "denivele_m",         &f)) cfg.denivele_m          = f;
        if (json_parse_bool (data, "mode_deg_vitesse",   &b)) cfg.mode_deg_vitesse    = b;
        if (json_parse_bool (data, "mode_deg_poumon",    &b)) cfg.mode_deg_poumon     = b;
        if (json_parse_int  (data, "machine_active",     &n)) cfg.machine_active      = n;
        config_nvs_sauver_machine(&cfg);
    }
}

// =============================================================================
// WebSocket handler
// =============================================================================

static esp_err_t ws_handler(httpd_req_t *req)
{
    if (req->method == HTTP_GET) {
        ESP_LOGI(TAG, "Client WS connecte fd=%d", httpd_req_to_sockfd(req));
        return ESP_OK;
    }

    uint8_t buf[512] = {0};
    httpd_ws_frame_t frame = {
        .type    = HTTPD_WS_TYPE_TEXT,
        .payload = buf,
    };

    esp_err_t ret = httpd_ws_recv_frame(req, &frame, sizeof(buf) - 1);
    if (ret != ESP_OK) return ret;

    if (frame.type == HTTPD_WS_TYPE_TEXT && frame.len > 0) {
        buf[frame.len] = '\0';
        handle_ws_command((char *)buf, frame.len);
    }
    return ESP_OK;
}

// =============================================================================
// HTTP root — Web UI embarquée
// =============================================================================

static esp_err_t root_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, (const char *)webui_html_start,
                    webui_html_end - webui_html_start);
    return ESP_OK;
}

// =============================================================================
// Broadcast WebSocket — httpd_queue_work (safe depuis tâche externe)
// =============================================================================

typedef struct {
    httpd_handle_t server;
    int            fd;
    char           json[JSON_BUF_SIZE];
    size_t         len;
} ws_send_arg_t;

static void ws_send_work_cb(void *arg)
{
    ws_send_arg_t *a = (ws_send_arg_t *)arg;
    httpd_ws_frame_t frame = {
        .type    = HTTPD_WS_TYPE_TEXT,
        .payload = (uint8_t *)a->json,
        .len     = a->len,
        .final   = true,
    };
    httpd_ws_send_frame_async(a->server, a->fd, &frame);
    free(a);
}

void webserver_broadcast_status(void)
{
    if (!s_server) return;

    machine_status_t s;
    state_machine_get_status(&s);

    char json[JSON_BUF_SIZE];
    int len = status_to_json(&s, json, sizeof(json));
    if (len <= 0 || len >= (int)sizeof(json)) return;

    size_t nb = MAX_WS_CLIENTS + 2;
    int fds[MAX_WS_CLIENTS + 2];
    if (httpd_get_client_list(s_server, &nb, fds) != ESP_OK) return;

    for (size_t i = 0; i < nb; i++) {
        if (httpd_ws_get_fd_info(s_server, fds[i]) != HTTPD_WS_CLIENT_WEBSOCKET) continue;

        ws_send_arg_t *arg = malloc(sizeof(ws_send_arg_t));
        if (!arg) continue;
        arg->server = s_server;
        arg->fd     = fds[i];
        memcpy(arg->json, json, len);
        arg->len    = (size_t)len;

        if (httpd_queue_work(s_server, ws_send_work_cb, arg) != ESP_OK) {
            free(arg);
        }
    }
}

// =============================================================================
// Init / Stop
// =============================================================================

esp_err_t webserver_init(void)
{
    wifi_ap_init();

    httpd_config_t cfg = HTTPD_DEFAULT_CONFIG();
    cfg.max_open_sockets = MAX_WS_CLIENTS + 2;

    esp_err_t ret = httpd_start(&s_server, &cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "httpd_start echoue : %s", esp_err_to_name(ret));
        return ret;
    }

    static const httpd_uri_t uri_ws = {
        .uri          = "/ws",
        .method       = HTTP_GET,
        .handler      = ws_handler,
        .is_websocket = true,
    };
    static const httpd_uri_t uri_root = {
        .uri     = "/",
        .method  = HTTP_GET,
        .handler = root_handler,
    };

    httpd_register_uri_handler(s_server, &uri_ws);
    httpd_register_uri_handler(s_server, &uri_root);
    ota_register_handler(s_server);

    ESP_LOGI(TAG, "Serveur HTTP/WebSocket demarre — http://192.168.4.1");
    return ESP_OK;
}

void webserver_stop(void)
{
    if (s_server) {
        httpd_stop(s_server);
        s_server = NULL;
    }
    esp_wifi_stop();
    ESP_LOGI(TAG, "Serveur et WiFi arretes");
}
