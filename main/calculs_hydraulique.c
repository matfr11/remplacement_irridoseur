#include "calculs_hydraulique.h"
#include "esp_log.h"
#include <math.h>
#include <stddef.h>

static const char *TAG = "calcul_hydro";

const float CANON_DOSES_MM[CANON_N_DOSES] = {10.0f, 15.0f, 20.0f, 25.0f, 30.0f};

// Table abaque constructeur Irrifrance — enrouleur Structure 1 bis, canon Nelson SR150
// 13 configurations buse/pression fournies par Irrifrance pour cet ensemble spécifique
const canon_entry_t CANON_TABLE[CANON_N_ENTRIES] = {
    {4.9f,23.0f,3.5f,17.3f,60.0f, { 9.6f,12.3f,15.3f,19.2f,25.6f}},
    {5.6f,24.6f,4.0f,17.3f,63.0f, { 9.8f,13.0f,15.6f,19.5f,26.0f}},
    {5.7f,29.8f,3.5f,20.3f,63.0f, {11.8f,15.8f,18.9f,23.7f,31.5f}},
    {6.5f,31.9f,4.0f,20.3f,66.0f, {12.1f,16.1f,19.3f,24.2f,32.2f}},
    {6.8f,37.8f,3.5f,22.9f,66.0f, {14.3f,19.1f,22.9f,28.6f,38.2f}},
    {6.9f,27.5f,5.0f,17.8f,66.0f, {10.4f,13.9f,16.7f,20.8f,27.8f}},
    {7.7f,40.4f,4.0f,22.9f,72.0f, {14.0f,18.7f,22.4f,28.1f,37.4f}},
    {8.0f,35.7f,5.0f,20.3f,72.0f, {12.4f,16.5f,19.8f,24.8f,33.1f}},
    {8.2f,30.1f,6.0f,17.8f,66.0f, {11.4f,15.2f,18.2f,22.8f,30.4f}},
    {8.4f,46.9f,3.5f,25.4f,72.0f, {16.3f,21.7f,26.1f,32.6f,43.4f}},
    {9.4f,50.1f,4.0f,25.4f,78.0f, {16.1f,21.4f,25.7f,32.1f,42.8f}},
    {9.5f,39.1f,6.0f,20.3f,72.0f, {13.6f,18.1f,21.7f,27.2f,36.2f}},
    {9.5f,45.2f,5.0f,22.9f,78.0f, {14.5f,19.3f,23.2f,29.0f,38.6f}},
};

// Plages utilisées pour la normalisation de la distance euclidienne
#define CANON_P_RANGE    4.6f   // 9.5 - 4.9 bar
#define CANON_BUSE_RANGE 2.5f   // 6.0 - 3.5 mm

float calcul_pression_canon(float p_mano_bar,
                             float longueur_tuyau_m,
                             float denivele_m)
{
    float perte_enrouleur = PERTE_ENROULEUR_DEFAULT_BAR;
    float perte_tuyau     = (longueur_tuyau_m / 300.0f) * PERTE_TUYAU_BAR_PAR_300M;
    float perte_denivele  = (denivele_m / 10.0f) * PERTE_DENIVELE_BAR_PAR_10M;

    float p_canon = p_mano_bar - perte_enrouleur - perte_tuyau - perte_denivele;
    return p_canon;
}

float lookup_vitesse_cible(float p_borne_bar, float buse_mm,
                            float dose_mm, float *debit_m3h_out)
{
    // Étape 1 : trouver l'entrée la plus proche par distance euclidienne normalisée
    int   best_idx  = 0;
    float best_dist = 1e30f;

    for (int i = 0; i < CANON_N_ENTRIES; i++) {
        float dp = (p_borne_bar - CANON_TABLE[i].p_borne_bar) / CANON_P_RANGE;
        float db = (buse_mm     - CANON_TABLE[i].buse_mm)     / CANON_BUSE_RANGE;
        float d2 = dp * dp + db * db;
        if (d2 < best_dist) {
            best_dist = d2;
            best_idx  = i;
        }
    }

    const canon_entry_t *e = &CANON_TABLE[best_idx];
    ESP_LOGD(TAG, "lookup: nearest entry %d (p=%.1f buse=%.1f)", best_idx,
             e->p_borne_bar, e->buse_mm);

    if (debit_m3h_out) {
        *debit_m3h_out = e->debit_m3h;
    }

    // Étape 2 : interpolation linéaire sur la dose
    if (dose_mm <= CANON_DOSES_MM[0]) {
        return e->vitesse_mh[0];
    }
    if (dose_mm >= CANON_DOSES_MM[CANON_N_DOSES - 1]) {
        return e->vitesse_mh[CANON_N_DOSES - 1];
    }

    for (int d = 0; d < CANON_N_DOSES - 1; d++) {
        if (dose_mm >= CANON_DOSES_MM[d] && dose_mm <= CANON_DOSES_MM[d + 1]) {
            float t = (dose_mm - CANON_DOSES_MM[d])
                      / (CANON_DOSES_MM[d + 1] - CANON_DOSES_MM[d]);
            return e->vitesse_mh[d] + t * (e->vitesse_mh[d + 1] - e->vitesse_mh[d]);
        }
    }

    return e->vitesse_mh[CANON_N_DOSES - 1];
}
