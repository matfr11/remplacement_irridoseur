#include "regulation.h"
#include "esp_log.h"
#include <string.h>

// Implémentation complète — PR-06
static const char *TAG = "regulation";

#define CALIB_WINDOW  5

static float  s_dist_buf[CALIB_WINDOW];
static int    s_buf_count   = 0;
static int    s_buf_idx     = 0;
static float  s_dist_moy    = 0.0f;
static int    s_nb_cycles   = 0;

float calcul_t_attente_s(float dist_par_cycle_m,
                          float vitesse_cible_m_s,
                          float t_remplissage_mesure_s,
                          float t_vidange_s,
                          bool  *alerte_pression_out)
{
    if (alerte_pression_out) *alerte_pression_out = false;

    if (dist_par_cycle_m < 1e-4f || vitesse_cible_m_s < 1e-6f) {
        return 0.0f;
    }

    float t_cycle_cible_s = dist_par_cycle_m / vitesse_cible_m_s;
    float t_attente = t_cycle_cible_s - t_remplissage_mesure_s - t_vidange_s;

    if (t_attente < 0.0f) {
        ESP_LOGW(TAG, "T_attente négatif (%.2fs) — pression insuffisante pour la dose", t_attente);
        if (alerte_pression_out) *alerte_pression_out = true;
        return 0.0f;
    }
    if (t_attente > 300.0f) {
        ESP_LOGW(TAG, "T_attente anormal (%.2fs > 300s)", t_attente);
    }

    return t_attente;
}

float correction_vitesse(float t_attente_s,
                          float vitesse_reelle_m_h,
                          float vitesse_cible_m_h,
                          float kp)
{
    if (vitesse_cible_m_h < 1e-3f) {
        return t_attente_s;
    }
    float erreur = vitesse_cible_m_h - vitesse_reelle_m_h;
    float correction = kp * (erreur / vitesse_cible_m_h) * t_attente_s;
    float t_corrige = t_attente_s + correction;
    return (t_corrige < 0.0f) ? 0.0f : t_corrige;
}

float regulation_update_dist_par_cycle(float nouvelle_dist_m)
{
    s_dist_buf[s_buf_idx] = nouvelle_dist_m;
    s_buf_idx = (s_buf_idx + 1) % CALIB_WINDOW;
    if (s_buf_count < CALIB_WINDOW) s_buf_count++;
    s_nb_cycles++;

    float somme = 0.0f;
    for (int i = 0; i < s_buf_count; i++) {
        somme += s_dist_buf[i];
    }
    s_dist_moy = somme / (float)s_buf_count;
    return s_dist_moy;
}

void regulation_reset_calibration(void)
{
    memset(s_dist_buf, 0, sizeof(s_dist_buf));
    s_buf_count = 0;
    s_buf_idx   = 0;
    s_dist_moy  = 0.0f;
    s_nb_cycles = 0;
}

int regulation_get_nb_cycles(void)
{
    return s_nb_cycles;
}
