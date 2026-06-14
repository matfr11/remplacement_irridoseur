#include "config_nvs.h"
#include "version.h"
#include "machines/machines.h"
#include "abaques/abaques.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_log.h"
#include <string.h>
#include <math.h>

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

// Chargement machine — NVS pour calibration terrain, machine/abaque verrouillés en PROD
esp_err_t config_nvs_charger_machine(config_machine_t *cfg)
{
    config_machine_t defaut = CFG_MACHINE_DEFAUT;
    *cfg = defaut;

    nvs_handle_t h;
    esp_err_t ret = nvs_open(NS_MACHINE, NVS_READONLY, &h);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Namespace machine absent — valeurs par défaut");
    } else {
        size_t sz = sizeof(config_machine_t);
        ret = nvs_get_blob(h, "cfg", cfg, &sz);
        nvs_close(h);
        if (ret == ESP_ERR_NVS_NOT_FOUND || ret == ESP_ERR_NVS_INVALID_LENGTH) {
            ESP_LOGW(TAG, "Config machine incompatible ou absente — valeurs par defaut");
            *cfg = defaut;
        } else if (ret != ESP_OK) {
            ESP_LOGW(TAG, "nvs_get_blob inattendu: 0x%x — valeurs par defaut", ret);
            *cfg = defaut;
        }
    }

    // Clamp t_ouv_canon_s — blob NVS corrompu peut contenir 0 ou valeur negative
    if (cfg->t_ouv_canon_s < 5.0f || cfg->t_ouv_canon_s > 60.0f)
        cfg->t_ouv_canon_s = 20.0f;

#ifdef CONFIG_IRRI_PROD
    // Verrouiller machine_active et abaque_idx sur les valeurs compilées
    #if defined(CONFIG_IRRI_MACHINE_ST1BIS_82_330)
        cfg->machine_active = MACHINE_IDX_ST1BIS_82_330;
    #else
        #error "IRRI_PROD activé mais aucun profil machine sélectionné"
    #endif
    #if defined(CONFIG_IRRI_CANON_SR100C)
        cfg->abaque_idx = ABAQUE_IDX_SR100C;
    #elif defined(CONFIG_IRRI_CANON_SR150C)
        cfg->abaque_idx = ABAQUE_IDX_SR150C;
    #else
        #error "IRRI_PROD activé mais aucun abaque canon sélectionné"
    #endif
    ESP_LOGI(TAG, "Mode PROD — machine %d / abaque %d v%s",
             cfg->machine_active, cfg->abaque_idx, IRRI_VERSION);
#else
    ESP_LOGI(TAG, "Mode DEV — profil machine depuis NVS (v%s)", IRRI_VERSION);
#endif
    return ESP_OK;
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
    if (ret == ESP_ERR_NVS_NOT_FOUND || ret == ESP_ERR_NVS_INVALID_LENGTH) return ESP_OK;
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

esp_err_t config_nvs_sauver_session_active(bool actif)
{
    nvs_handle_t h;
    esp_err_t ret = nvs_open(NS_STATE, NVS_READWRITE, &h);
    if (ret != ESP_OK) return ret;
    ret = nvs_set_u8(h, "session_act", actif ? 1u : 0u);
    if (ret == ESP_OK) ret = nvs_commit(h);
    nvs_close(h);
    return ret;
}

bool config_nvs_lire_session_active(void)
{
    nvs_handle_t h;
    if (nvs_open(NS_STATE, NVS_READONLY, &h) != ESP_OK) return false;
    uint8_t val = 0;
    nvs_get_u8(h, "session_act", &val);
    nvs_close(h);
    return val != 0;
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

esp_err_t config_nvs_sauver_position(float longueur_m, float deroule_m)
{
    nvs_handle_t h;
    esp_err_t ret = nvs_open(NS_STATE, NVS_READWRITE, &h);
    if (ret != ESP_OK) return ret;
    ret = nvs_set_blob(h, "longueur_m", &longueur_m, sizeof(float));
    if (ret == ESP_OK) ret = nvs_set_blob(h, "deroule_m", &deroule_m, sizeof(float));
    if (ret == ESP_OK) ret = nvs_commit(h);
    nvs_close(h);
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

esp_err_t config_nvs_lire_batt_seuils(float *warn_v, float *crit_v)
{
    *warn_v = 11.5f;
    *crit_v = 11.0f;
    nvs_handle_t h;
    if (nvs_open(NS_MACHINE, NVS_READONLY, &h) != ESP_OK) return ESP_OK;
    float v;
    size_t sz;
    sz = sizeof(float);
    if (nvs_get_blob(h, "batt_warn", &v, &sz) == ESP_OK && isfinite(v) && v > 10.0f && v < 15.0f)
        *warn_v = v;
    sz = sizeof(float);
    if (nvs_get_blob(h, "batt_crit", &v, &sz) == ESP_OK && isfinite(v) && v > 10.0f && v < 15.0f)
        *crit_v = v;
    nvs_close(h);
    return ESP_OK;
}

esp_err_t config_nvs_sauver_batt_seuils(float warn_v, float crit_v)
{
    nvs_handle_t h;
    esp_err_t ret = nvs_open(NS_MACHINE, NVS_READWRITE, &h);
    if (ret != ESP_OK) return ret;
    ret = nvs_set_blob(h, "batt_warn", &warn_v, sizeof(float));
    if (ret == ESP_OK) ret = nvs_set_blob(h, "batt_crit", &crit_v, sizeof(float));
    if (ret == ESP_OK) ret = nvs_commit(h);
    nvs_close(h);
    return ret;
}

esp_err_t config_nvs_lire_t_rempl_min(float *t_s)
{
    *t_s = 5.0f;
    nvs_handle_t h;
    if (nvs_open(NS_MACHINE, NVS_READONLY, &h) != ESP_OK) return ESP_OK;
    size_t sz = sizeof(float);
    float v;
    if (nvs_get_blob(h, "t_rempl_min", &v, &sz) == ESP_OK && isfinite(v) && v > 0.5f && v < 120.0f)
        *t_s = v;
    nvs_close(h);
    return ESP_OK;
}

esp_err_t config_nvs_sauver_t_rempl_min(float t_s)
{
    nvs_handle_t h;
    esp_err_t ret = nvs_open(NS_MACHINE, NVS_READWRITE, &h);
    if (ret != ESP_OK) return ret;
    ret = nvs_set_blob(h, "t_rempl_min", &t_s, sizeof(float));
    if (ret == ESP_OK) ret = nvs_commit(h);
    nvs_close(h);
    return ret;
}
