#include "config_nvs.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_log.h"
#include <string.h>
#include <stdio.h>

static const char *TAG = "config_nvs";

// Valeur par défaut T_vidange : 5s (mécanique ressort — à affiner terrain)
#define T_VIDANGE_DEFAUT_S   5.0f

esp_err_t config_nvs_init(void)
{
    nvs_handle_t h;
    esp_err_t err = nvs_open(NVS_NS_MACHINE, NVS_READWRITE, &h);
    if (err == ESP_OK) {
        nvs_close(h);
        ESP_LOGI(TAG, "Namespace '%s' accessible", NVS_NS_MACHINE);
    } else {
        ESP_LOGW(TAG, "Namespace '%s' absent — valeurs par défaut utilisées", NVS_NS_MACHINE);
    }
    return ESP_OK;
}

esp_err_t config_nvs_lire_machine(config_machine_t *cfg)
{
    // Valeurs par défaut — géométrie issue fiche technique Structure 2 AT/P
    cfg->longueur_tuyau_m  = 330.0f;
    cfg->diam_int_tuyau_m  = 0.082f;
    cfg->d_tuyau_ext_m     = 0.094f;   // Défaut : 82mm int + 2×6mm ep. = 94mm — modifiable UI
    cfg->r_tambour_vide_m  = 0.648f;   // Défaut : tableau spires ST.1 Bis — modifiable UI
    cfg->nb_etages         = 4;
    cfg->t_vidange_s       = T_VIDANGE_DEFAUT_S;  // ⚠️ à mesurer terrain
    cfg->kp_regulation     = 0.1f;
    cfg->n_cycles_calib    = 3;
    cfg->perte_enroul_bar  = 2.5f;
    cfg->denivele_m        = 0.0f;     // Terrain plat (installation de référence)

    // Sens contacteurs — défaut NO (actif bas) selon schéma câblage SPECS.md
    // À vérifier au multimètre avant mise en service
    cfg->contact_no_fin_course      = true;
    cfg->contact_no_securite_spires = true;
    cfg->contact_no_poumon          = true;
    cfg->contact_no_pressostat      = true;

    nvs_handle_t h;
    esp_err_t err = nvs_open(NVS_NS_MACHINE, NVS_READONLY, &h);
    if (err != ESP_OK) {
        return ESP_OK;  // Namespace absent — valeurs par défaut
    }

    // TODO PR-06 : lire chaque clé depuis NVS et écraser les défauts
    // nvs_get_blob(h, "d_tuyau_ext",  &cfg->d_tuyau_ext_m, sizeof(float));
    // nvs_get_blob(h, "r_tambour",    &cfg->r_tambour_vide_m, sizeof(float));
    // nvs_get_blob(h, "t_vidange_s",  &cfg->t_vidange_s, sizeof(float));
    // nvs_get_blob(h, "kp_reg",       &cfg->kp_regulation, sizeof(float));
    // nvs_get_i32 (h, "n_calib",      &val) → cfg->n_cycles_calib
    // nvs_get_u8  (h, "no_fin_crs",   &u8)  → cfg->contact_no_fin_course
    // nvs_get_u8  (h, "no_secu_spi",  &u8)  → cfg->contact_no_securite_spires
    // nvs_get_u8  (h, "no_poumon",    &u8)  → cfg->contact_no_poumon
    // nvs_get_u8  (h, "no_pressost",  &u8)  → cfg->contact_no_pressostat

    nvs_close(h);
    return ESP_OK;
}

esp_err_t config_nvs_sauver_machine(const config_machine_t *cfg)
{
    // TODO PR-06 : implémentation complète
    (void)cfg;
    ESP_LOGW(TAG, "config_nvs_sauver_machine — stub (implémenté PR-06)");
    return ESP_OK;
}

esp_err_t config_nvs_lire_programme(int idx, config_programme_t *prog)
{
    if (idx < 0 || idx >= NB_PROGRAMMES) {
        return ESP_ERR_INVALID_ARG;
    }

    // Valeurs par défaut
    snprintf(prog->nom, NOM_PROGRAMME_MAX, "Programme %d", idx + 1);
    prog->dose_mm          = 25.0f;
    prog->largeur_m        = 18.0f;
    prog->d_buse_mm        = 18;
    prog->p_borne_bar      = 6.0f;
    prog->tempo_depart_on  = false;
    prog->tempo_depart_s   = 0;
    prog->tempo_arrivee_on = false;
    prog->tempo_arrivee_s  = 0;

    // TODO PR-06 : lecture depuis NVS namespace irri_prog, clés p{idx}_{champ}

    return ESP_OK;
}

esp_err_t config_nvs_sauver_programme(int idx, const config_programme_t *prog)
{
    if (idx < 0 || idx >= NB_PROGRAMMES) {
        return ESP_ERR_INVALID_ARG;
    }
    (void)prog;
    ESP_LOGW(TAG, "config_nvs_sauver_programme — stub (implémenté PR-06)");
    return ESP_OK;
}

esp_err_t config_nvs_lire_prog_actif(int *idx)
{
    *idx = 0;

    nvs_handle_t h;
    esp_err_t err = nvs_open(NVS_NS_STATE, NVS_READONLY, &h);
    if (err != ESP_OK) {
        return ESP_OK;
    }

    int32_t val = 0;
    if (nvs_get_i32(h, "prog_actif", &val) == ESP_OK) {
        if (val >= 0 && val < NB_PROGRAMMES) {
            *idx = (int)val;
        }
    }
    nvs_close(h);
    return ESP_OK;
}

esp_err_t config_nvs_sauver_prog_actif(int idx)
{
    if (idx < 0 || idx >= NB_PROGRAMMES) {
        return ESP_ERR_INVALID_ARG;
    }

    nvs_handle_t h;
    esp_err_t err = nvs_open(NVS_NS_STATE, NVS_READWRITE, &h);
    if (err != ESP_OK) {
        return err;
    }
    err = nvs_set_i32(h, "prog_actif", (int32_t)idx);
    if (err == ESP_OK) {
        err = nvs_commit(h);
    }
    nvs_close(h);
    return err;
}

bool config_machine_est_valide(const config_machine_t *cfg)
{
    // La géométrie (r_tambour, d_tuyau_ext) est maintenant connue via fiche technique.
    // Seul t_vidange_s doit être non nul pour autoriser le démarrage.
    // Avec la valeur par défaut 5.0s, le démarrage est autorisé même sans mesure terrain.
    return (cfg->t_vidange_s > 0.0f);
}

bool config_machine_t_vidange_par_defaut(const config_machine_t *cfg)
{
    // Retourne true si T_vidange est encore à la valeur théorique par défaut.
    // L'UI peut afficher un avertissement "⚠️ T_vidange non mesuré — valeur théorique 5s".
    return (cfg->t_vidange_s == T_VIDANGE_DEFAUT_S);
}
