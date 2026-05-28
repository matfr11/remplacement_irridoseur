#include "calculs_mecanique.h"
#include "esp_log.h"
#include <math.h>

static const char *TAG = "calcul_meca";

#define M_PI_F  3.14159265358979f

// --- Fonctions géométrie bobine (inchangées depuis PR-01) ---

float calcul_rayon_etage(int n, float r_tambour_vide_m, float d_tuyau_ext_m)
{
    return r_tambour_vide_m + ((float)n - 0.5f) * d_tuyau_ext_m;
}

int calcul_etage_courant(uint32_t nb_impulsions_total,
                          float r_tambour_vide_m,
                          float d_tuyau_ext_m,
                          float longueur_tuyau_m,
                          int nb_etages)
{
    if (r_tambour_vide_m <= 0.0f || d_tuyau_ext_m <= 0.0f || nb_etages <= 0) {
        return 1;
    }

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
    return (2.0f * M_PI_F * r_etage_m) / (float)NB_PASTILLES;
}

// --- Calibration capteur vitesse ---

float calcul_longueur_depuis_impulsions(uint32_t nb_impulsions,
                                         float r_tambour_vide_m,
                                         float d_tuyau_ext_m,
                                         float longueur_tuyau_m,
                                         int   nb_etages)
{
    if (r_tambour_vide_m <= 0.0f || d_tuyau_ext_m <= 0.0f
            || nb_etages <= 0 || longueur_tuyau_m <= 0.0f) {
        return 0.0f;
    }

    float longueur_par_etage = longueur_tuyau_m / (float)nb_etages;
    float longueur           = 0.0f;
    float remaining          = (float)nb_impulsions;

    for (int n = 1; n <= nb_etages && remaining > 0.0f; n++) {
        float r_n          = calcul_rayon_etage(n, r_tambour_vide_m, d_tuyau_ext_m);
        float dist_pulse_n = (2.0f * M_PI_F * r_n) / (float)NB_PASTILLES;
        float imp_etage_n  = longueur_par_etage / dist_pulse_n;

        float used = (remaining < imp_etage_n) ? remaining : imp_etage_n;
        longueur  += used * dist_pulse_n;
        remaining -= used;
    }

    // Impulsions excédentaires (au-delà de la longueur nominale) : dernier étage
    if (remaining > 0.0f) {
        float r_last     = calcul_rayon_etage(nb_etages, r_tambour_vide_m, d_tuyau_ext_m);
        float dist_last  = (2.0f * M_PI_F * r_last) / (float)NB_PASTILLES;
        longueur        += remaining * dist_last;
    }

    return longueur;
}

float calcul_correction_capteur(float longueur_reelle_m, float longueur_calculee_m)
{
    if (longueur_reelle_m <= 0.0f) {
        ESP_LOGW(TAG, "calibration: longueur réelle invalide (%.2f)", longueur_reelle_m);
        return -1.0f;
    }
    if (longueur_calculee_m < 5.0f) {
        ESP_LOGW(TAG, "calibration: longueur calculée insuffisante (%.2fm) — capteur défaillant",
                 longueur_calculee_m);
        return -1.0f;
    }
    float k = longueur_reelle_m / longueur_calculee_m;
    if (k < 0.5f || k > 2.0f) {
        ESP_LOGW(TAG, "calibration: facteur k=%.3f hors bornes [0.5, 2.0] — rejeté", k);
        return -1.0f;
    }
    return k;
}

// --- Moyenne glissante dist_par_cycle (fenêtre 5 cycles) ---

#define DIST_CYCLE_WINDOW 5
static float s_dist_buf[DIST_CYCLE_WINDOW] = {0};
static int   s_dist_idx = 0;
static int   s_dist_n   = 0;

void update_dist_par_cycle(float nouvelle_mesure_m)
{
    s_dist_buf[s_dist_idx % DIST_CYCLE_WINDOW] = nouvelle_mesure_m;
    s_dist_idx++;
    if (s_dist_n < DIST_CYCLE_WINDOW) {
        s_dist_n++;
    }
}

float get_dist_par_cycle_m(void)
{
    if (s_dist_n == 0) {
        return 0.0f;
    }
    float sum = 0.0f;
    for (int i = 0; i < s_dist_n; i++) {
        sum += s_dist_buf[i];
    }
    return sum / (float)s_dist_n;
}

// --- Régulation feedforward ---

float calcul_t_attente_s(float dist_par_cycle_m,
                          float vitesse_cible_m_s,
                          float t_remplissage_mesure_s,
                          float t_vidange_s)
{
    if (vitesse_cible_m_s <= 0.0f || dist_par_cycle_m <= 0.0f) {
        ESP_LOGE(TAG, "calcul_t_attente: paramètres invalides dist=%.3f v=%.6f",
                 dist_par_cycle_m, vitesse_cible_m_s);
        return 0.0f;
    }

    float t_cycle  = dist_par_cycle_m / vitesse_cible_m_s;
    float t_attente = t_cycle - t_remplissage_mesure_s - t_vidange_s;

    if (t_attente > 300.0f) {
        ESP_LOGW(TAG, "calcul_t_attente: T_attente=%.1fs anormalement long", t_attente);
    }
    if (t_attente < 0.0f) {
        return 0.0f;
    }
    return t_attente;
}

float correction_vitesse(float t_attente_actuel_s,
                          float vitesse_reelle_m_h,
                          float vitesse_cible_m_h,
                          float kp)
{
    float erreur     = vitesse_reelle_m_h - vitesse_cible_m_h;
    float correction = erreur * kp;
    float t_nouveau  = t_attente_actuel_s + correction;
    return (t_nouveau < 0.0f) ? 0.0f : t_nouveau;
}
