#include "config_nvs.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_log.h"
#include <string.h>

// Implémentation complète — PR-07
static const char *TAG = "config_nvs";

#define NS_MACHINE  "irri_machine"
#define NS_STATE    "irri_state"
#define NS_PROG_FMT "irri_prog%d"     // index 0-4

esp_err_t config_nvs_init(void)
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_LOGW(TAG, "NVS flash erase requis");
        nvs_flash_erase();
        ret = nvs_flash_init();
    }
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "nvs_flash_init échoué : %s", esp_err_to_name(ret));
    }
    return ret;
}

// Lecture machine — retourne CFG_MACHINE_DEFAUT si clé absente
esp_err_t config_nvs_lire_machine(config_machine_t *cfg)
{
    config_machine_t defaut = CFG_MACHINE_DEFAUT;
    *cfg = defaut;

    nvs_handle_t h;
    esp_err_t ret = nvs_open(NS_MACHINE, NVS_READONLY, &h);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Namespace machine absent — valeurs par défaut");
        return ESP_OK;
    }

    size_t sz = sizeof(config_machine_t);
    ret = nvs_get_blob(h, "cfg", cfg, &sz);
    nvs_close(h);

    if (ret == ESP_ERR_NVS_NOT_FOUND || ret == ESP_ERR_NVS_INVALID_LENGTH) {
        // Blob absent ou taille différente (migration firmware) → valeurs par défaut
        ESP_LOGW(TAG, "Config machine incompatible ou absente — valeurs par defaut");
        *cfg = defaut;
        return ESP_OK;
    }
    return ret;
}

esp_err_t config_nvs_sauver_machine(const config_machine_t *cfg)
{
    nvs_handle_t h;
    esp_err_t ret = nvs_open(NS_MACHINE, NVS_READWRITE, &h);
    if (ret != ESP_OK) return ret;
    ret = nvs_set_blob(h, "cfg", cfg, sizeof(config_machine_t));
    if (ret == ESP_OK) ret = nvs_commit(h);
    nvs_close(h);
    return ret;
}

esp_err_t config_nvs_lire_programme(int idx, config_programme_t *prog)
{
    if (idx < 0 || idx >= NB_PROGRAMMES) return ESP_ERR_INVALID_ARG;
    memset(prog, 0, sizeof(*prog));

    char ns[16];
    snprintf(ns, sizeof(ns), NS_PROG_FMT, idx);

    nvs_handle_t h;
    esp_err_t ret = nvs_open(ns, NVS_READONLY, &h);
    if (ret != ESP_OK) return ESP_OK;  // programme vide

    size_t sz = sizeof(config_programme_t);
    ret = nvs_get_blob(h, "prog", prog, &sz);
    nvs_close(h);
    if (ret == ESP_ERR_NVS_NOT_FOUND) return ESP_OK;
    return ret;
}

esp_err_t config_nvs_sauver_programme(int idx, const config_programme_t *prog)
{
    if (idx < 0 || idx >= NB_PROGRAMMES) return ESP_ERR_INVALID_ARG;

    char ns[16];
    snprintf(ns, sizeof(ns), NS_PROG_FMT, idx);

    nvs_handle_t h;
    esp_err_t ret = nvs_open(ns, NVS_READWRITE, &h);
    if (ret != ESP_OK) return ret;
    ret = nvs_set_blob(h, "prog", prog, sizeof(config_programme_t));
    if (ret == ESP_OK) ret = nvs_commit(h);
    nvs_close(h);
    return ret;
}

esp_err_t config_nvs_lire_prog_actif(int *idx)
{
    nvs_handle_t h;
    *idx = 0;
    esp_err_t ret = nvs_open(NS_STATE, NVS_READONLY, &h);
    if (ret != ESP_OK) return ESP_OK;
    int32_t val = 0;
    ret = nvs_get_i32(h, "prog_actif", &val);
    nvs_close(h);
    if (ret == ESP_ERR_NVS_NOT_FOUND) return ESP_OK;
    *idx = (int)val;
    return ret;
}

esp_err_t config_nvs_sauver_prog_actif(int idx)
{
    nvs_handle_t h;
    esp_err_t ret = nvs_open(NS_STATE, NVS_READWRITE, &h);
    if (ret != ESP_OK) return ret;
    ret = nvs_set_i32(h, "prog_actif", (int32_t)idx);
    if (ret == ESP_OK) ret = nvs_commit(h);
    nvs_close(h);
    return ret;
}

esp_err_t config_nvs_sauver_urgence(const char *raison)
{
    nvs_handle_t h;
    esp_err_t ret = nvs_open(NS_STATE, NVS_READWRITE, &h);
    if (ret != ESP_OK) return ret;
    ret = nvs_set_str(h, "urgence", raison);
    if (ret == ESP_OK) ret = nvs_commit(h);
    nvs_close(h);
    return ret;
}

esp_err_t config_nvs_lire_urgence(char *raison, size_t len)
{
    nvs_handle_t h;
    raison[0] = '\0';
    esp_err_t ret = nvs_open(NS_STATE, NVS_READONLY, &h);
    if (ret != ESP_OK) return ESP_OK;
    ret = nvs_get_str(h, "urgence", raison, &len);
    nvs_close(h);
    if (ret == ESP_ERR_NVS_NOT_FOUND) return ESP_OK;
    return ret;
}

esp_err_t config_nvs_sauver_longueur(float longueur_m)
{
    nvs_handle_t h;
    esp_err_t ret = nvs_open(NS_STATE, NVS_READWRITE, &h);
    if (ret != ESP_OK) return ret;
    ret = nvs_set_blob(h, "longueur_m", &longueur_m, sizeof(float));
    if (ret == ESP_OK) ret = nvs_commit(h);
    nvs_close(h);
    return ret;
}

esp_err_t config_nvs_lire_longueur(float *longueur_m)
{
    *longueur_m = 0.0f;
    nvs_handle_t h;
    esp_err_t ret = nvs_open(NS_STATE, NVS_READONLY, &h);
    if (ret != ESP_OK) return ESP_OK;
    size_t sz = sizeof(float);
    ret = nvs_get_blob(h, "longueur_m", longueur_m, &sz);
    nvs_close(h);
    if (ret == ESP_ERR_NVS_NOT_FOUND) return ESP_OK;
    return ret;
}

esp_err_t config_nvs_sauver_deroule(float deroule_m)
{
    nvs_handle_t h;
    esp_err_t ret = nvs_open(NS_STATE, NVS_READWRITE, &h);
    if (ret != ESP_OK) return ret;
    ret = nvs_set_blob(h, "deroule_m", &deroule_m, sizeof(float));
    if (ret == ESP_OK) ret = nvs_commit(h);
    nvs_close(h);
    return ret;
}

esp_err_t config_nvs_lire_deroule(float *deroule_m)
{
    *deroule_m = 0.0f;
    nvs_handle_t h;
    esp_err_t ret = nvs_open(NS_STATE, NVS_READONLY, &h);
    if (ret != ESP_OK) return ESP_OK;
    size_t sz = sizeof(float);
    ret = nvs_get_blob(h, "deroule_m", deroule_m, &sz);
    nvs_close(h);
    if (ret == ESP_ERR_NVS_NOT_FOUND) return ESP_OK;
    return ret;
}

bool config_programme_est_valide(const config_programme_t *prog)
{
    return prog->dose_mm > 0.0f  &&
           prog->largeur_m > 0.0f &&
           prog->buse_mm > 0      &&
           prog->pression_bar > 0.0f;
}

#define NS_STATS "irri_stats"

esp_err_t config_nvs_lire_stats(config_stats_t *stats)
{
    memset(stats, 0, sizeof(*stats));
    nvs_handle_t h;
    esp_err_t ret = nvs_open(NS_STATS, NVS_READONLY, &h);
    if (ret != ESP_OK) return ESP_OK;
    size_t sz = sizeof(config_stats_t);
    ret = nvs_get_blob(h, "camp", stats, &sz);
    nvs_close(h);
    if (ret == ESP_ERR_NVS_NOT_FOUND || ret == ESP_ERR_NVS_INVALID_LENGTH) {
        memset(stats, 0, sizeof(*stats));
        return ESP_OK;
    }
    return ret;
}

esp_err_t config_nvs_sauver_stats(const config_stats_t *stats)
{
    nvs_handle_t h;
    esp_err_t ret = nvs_open(NS_STATS, NVS_READWRITE, &h);
    if (ret != ESP_OK) return ret;
    ret = nvs_set_blob(h, "camp", stats, sizeof(config_stats_t));
    if (ret == ESP_OK) ret = nvs_commit(h);
    nvs_close(h);
    return ret;
}

esp_err_t config_nvs_reset_stats(void)
{
    config_stats_t z;
    memset(&z, 0, sizeof(z));
    return config_nvs_sauver_stats(&z);
}
