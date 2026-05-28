#include "calculs_mecanique.h"
#include "esp_log.h"
#include <math.h>

static const char *TAG = "calcul_meca";

#define M_PI_F  3.14159265358979f

float calcul_rayon_etage(int n, float r_tambour_vide_m, float d_tuyau_ext_m)
{
    // R_n = R_tambour_vide + (n - 0.5) × d_tuyau_ext
    // Le centre de la spire à l'étage n est à (n-0.5) × d_tuyau du tambour
    return r_tambour_vide_m + ((float)n - 0.5f) * d_tuyau_ext_m;
}

int calcul_etage_courant(uint32_t nb_impulsions_total,
                          float r_tambour_vide_m,
                          float d_tuyau_ext_m,
                          float longueur_tuyau_m,
                          int nb_etages)
{
    if (r_tambour_vide_m <= 0.0f || d_tuyau_ext_m <= 0.0f || nb_etages <= 0) {
        // Paramètres non mesurés — retour étage 1 par sécurité
        return 1;
    }

    // Longueur de tuyau enroulée par étage (distribution uniforme)
    float longueur_par_etage = longueur_tuyau_m / (float)nb_etages;
    float impulsions_cumulees = 0.0f;

    for (int n = 1; n <= nb_etages; n++) {
        float r_n           = calcul_rayon_etage(n, r_tambour_vide_m, d_tuyau_ext_m);
        float circonference = 2.0f * M_PI_F * r_n;
        float tours_etage   = longueur_par_etage / circonference;
        impulsions_cumulees += tours_etage * (float)NB_PASTILLES;

        if ((float)nb_impulsions_total <= impulsions_cumulees) {
            return n;
        }
    }
    return nb_etages;
}

float calcul_dist_pulse_m(float r_etage_m)
{
    // Un tour complet couvre 2π × R, divisé par le nombre de pastilles
    return (2.0f * M_PI_F * r_etage_m) / (float)NB_PASTILLES;
}

float calcul_dist_poumon_m(float r_etage_m, float alpha_deg)
{
    // L'angle alpha correspond à une fraction du périmètre
    return (alpha_deg / 360.0f) * 2.0f * M_PI_F * r_etage_m;
}

float calcul_freq_poumon(float vitesse_cible_m_min, float dist_poumon_m)
{
    if (dist_poumon_m <= 0.0f) {
        ESP_LOGE(TAG, "dist_poumon invalide : %.4fm (sera remplacé par auto-calibration PR-04)", dist_poumon_m);
        return 0.0f;
    }
    return vitesse_cible_m_min / dist_poumon_m;
}
