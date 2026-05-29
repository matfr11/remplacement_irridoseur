#include "ota.h"
#include "state_machine.h"
#include "esp_ota_ops.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>

static const char *TAG = "ota";

#define OTA_BUF_SIZE  1024

static esp_err_t ota_update_handler(httpd_req_t *req)
{
    if (state_machine_get_etat() != ETAT_VEILLE) {
        const char *msg = "{\"error\":\"Arreter le cycle avant la mise a jour\"}";
        httpd_resp_set_status(req, "409 Conflict");
        httpd_resp_set_type(req, "application/json");
        httpd_resp_send(req, msg, strlen(msg));
        return ESP_OK;
    }

    const esp_partition_t *update_part = esp_ota_get_next_update_partition(NULL);
    if (!update_part) {
        ESP_LOGE(TAG, "Partition OTA introuvable");
        httpd_resp_send_500(req);
        return ESP_OK;
    }

    esp_ota_handle_t ota_handle;
    esp_err_t ret = esp_ota_begin(update_part, OTA_WITH_SEQUENTIAL_WRITES, &ota_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "esp_ota_begin : %s", esp_err_to_name(ret));
        httpd_resp_send_500(req);
        return ESP_OK;
    }

    ESP_LOGI(TAG, "OTA : reception firmware (%d octets)", req->content_len);

    char buf[OTA_BUF_SIZE];
    int remaining = req->content_len;
    while (remaining > 0) {
        int chunk = (remaining < OTA_BUF_SIZE) ? remaining : OTA_BUF_SIZE;
        int received = httpd_req_recv(req, buf, chunk);
        if (received <= 0) {
            ESP_LOGE(TAG, "Erreur reception chunk");
            esp_ota_abort(ota_handle);
            httpd_resp_send_500(req);
            return ESP_OK;
        }
        ret = esp_ota_write(ota_handle, buf, received);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "esp_ota_write : %s", esp_err_to_name(ret));
            esp_ota_abort(ota_handle);
            httpd_resp_send_500(req);
            return ESP_OK;
        }
        remaining -= received;
    }

    ret = esp_ota_end(ota_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "esp_ota_end : %s", esp_err_to_name(ret));
        httpd_resp_send_500(req);
        return ESP_OK;
    }

    ret = esp_ota_set_boot_partition(update_part);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "esp_ota_set_boot_partition : %s", esp_err_to_name(ret));
        httpd_resp_send_500(req);
        return ESP_OK;
    }

    ESP_LOGI(TAG, "OTA reussie — redemarrage dans 3s");
    const char *ok = "{\"status\":\"ok\",\"message\":\"Mise a jour reussie — Redemarrage dans 3s\"}";
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, ok, strlen(ok));

    vTaskDelay(pdMS_TO_TICKS(3000));
    esp_restart();
    return ESP_OK;
}

esp_err_t ota_init(void)
{
    ESP_LOGI(TAG, "OTA pret");
    return ESP_OK;
}

void ota_register_handler(httpd_handle_t server)
{
    static const httpd_uri_t uri_ota = {
        .uri     = "/ota/update",
        .method  = HTTP_POST,
        .handler = ota_update_handler,
    };
    httpd_register_uri_handler(server, &uri_ota);
    ESP_LOGI(TAG, "Handler OTA enregistre sur POST /ota/update");
}
