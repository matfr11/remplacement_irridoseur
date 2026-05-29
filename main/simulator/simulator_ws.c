#include "simulator_ws.h"

#ifdef CONFIG_IRRI_TEST_MODE

#include "simulator.h"
#include "simulator_ui.h"
#include "gpio_config.h"
#include "state_machine.h"
#include "esp_log.h"
#include "esp_http_server.h"
#include <string.h>
#include <stdlib.h>

static const char *TAG = "sim_ws";

// Parsing minimaliste JSON {"sim":"<key>","value":<0|1>}
// Retourne true si clé/valeur extraits
static bool parse_sim_cmd(const char *json, char *key_out, int key_max, int *val_out)
{
    const char *p = strstr(json, "\"sim\"");
    if (!p) return false;
    p = strchr(p + 5, '"');
    if (!p) return false;
    p++;
    const char *end = strchr(p, '"');
    if (!end || (end - p) >= key_max) return false;
    memcpy(key_out, p, end - p);
    key_out[end - p] = '\0';

    const char *v = strstr(json, "\"value\"");
    if (!v) return false;
    v = strchr(v + 7, ':');
    if (!v) return false;
    *val_out = atoi(v + 1);
    return true;
}

// Parsing JSON {"scenario":<n>}
static int parse_scenario(const char *json)
{
    const char *p = strstr(json, "\"scenario\"");
    if (!p) return -1;
    p = strchr(p + 10, ':');
    if (!p) return -1;
    return atoi(p + 1);
}

static void apply_sim_key(const char *key, int val)
{
    if      (strcmp(key, "pressostat")   == 0) sim_gpio_set_level(PIN_PRESSOSTAT,   val);
    else if (strcmp(key, "fin_course")   == 0) sim_gpio_set_level(PIN_FIN_COURSE,   val);
    else if (strcmp(key, "secu_spires")  == 0) sim_gpio_set_level(PIN_SECU_SPIRES,  val);
    else if (strcmp(key, "poumon_plein") == 0) sim_gpio_set_level(PIN_POUMON_PLEIN, val);
    else if (strcmp(key, "vitesse_auto") == 0) sim_set_vitesse_auto((float)val);
    else ESP_LOGW(TAG, "Clé inconnue: %s", key);
}

static void run_scenario(int n)
{
    ESP_LOGI(TAG, "Scénario %d", n);
    switch (n) {
    case 1:  // cycle normal — reset + pression ok
        state_machine_test_reset();
        sim_gpio_set_level(PIN_PRESSOSTAT, 0);
        sim_gpio_set_level(PIN_FIN_COURSE, 0);
        sim_gpio_set_level(PIN_SECU_SPIRES, 0);
        break;
    case 5:  // urgence spires
        sim_gpio_set_level(PIN_SECU_SPIRES, 1);
        break;
    case 6:  // urgence fin_course
        sim_gpio_set_level(PIN_FIN_COURSE, 1);
        break;
    case 3:  // perte pression
        sim_gpio_set_level(PIN_PRESSOSTAT, 1);
        break;
    case 4:  // reprise pression
        sim_gpio_set_level(PIN_PRESSOSTAT, 0);
        break;
    default:
        ESP_LOGW(TAG, "Scénario %d non implémenté", n);
        break;
    }
}

static esp_err_t ws_test_handler(httpd_req_t *req)
{
    if (req->method == HTTP_GET) {
        ESP_LOGI(TAG, "Client sim WS connecte fd=%d", httpd_req_to_sockfd(req));
        return ESP_OK;
    }

    uint8_t buf[256] = {0};
    httpd_ws_frame_t frame = { .type = HTTPD_WS_TYPE_TEXT, .payload = buf, .len = 0 };
    esp_err_t ret = httpd_ws_recv_frame(req, &frame, sizeof(buf) - 1);
    if (ret != ESP_OK) return ret;

    buf[frame.len] = '\0';
    ESP_LOGI(TAG, "WS sim cmd: %s", (char *)buf);

    char key[32];
    int  val = 0;
    if (parse_sim_cmd((char *)buf, key, sizeof(key), &val)) {
        apply_sim_key(key, val);
    } else {
        int sc = parse_scenario((char *)buf);
        if (sc >= 0) run_scenario(sc);
    }
    return ESP_OK;
}

static esp_err_t test_page_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, (const char *)simulator_ui_html_start,
                    simulator_ui_html_end - simulator_ui_html_start);
    return ESP_OK;
}

void simulator_ws_register(httpd_handle_t server)
{
    static const httpd_uri_t uri_ws_test = {
        .uri          = "/ws_test",
        .method       = HTTP_GET,
        .handler      = ws_test_handler,
        .is_websocket = true,
    };
    static const httpd_uri_t uri_test = {
        .uri    = "/test",
        .method = HTTP_GET,
        .handler = test_page_handler,
    };
    httpd_register_uri_handler(server, &uri_ws_test);
    httpd_register_uri_handler(server, &uri_test);
    ESP_LOGI(TAG, "Routes simulateur enregistrees: /test, /ws_test");
}

#endif // CONFIG_IRRI_TEST_MODE
