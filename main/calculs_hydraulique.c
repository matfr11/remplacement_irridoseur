#include "calculs_hydraulique.h"
#include "esp_log.h"
#include <math.h>

static const char *TAG = "calcul_hydro";

// Table débit canon (m³/h) — source : UASA46, "Choisir le bon diamètre de buse", 19/06/2025
// Lignes  : pression canon 4.0 / 4.5 / 5.0 / 5.5 / 6.0 bar
// Colonnes: diamètre buse 12 / 13 / 14 / 15 / 16 / 17 / 18 / 19 / 20 / 21 / 22 / 23 / 24 / 25 / 26 mm
const float DEBIT_TABLE_M3H[5][15] = {
    /* 4.0 bar */ {10.8f,12.7f,14.7f,16.9f,19.2f,21.7f,24.3f,27.1f,30.0f,33.1f,36.4f,39.8f,43.3f,47.0f,50.8f},
    /* 4.5 bar */ {11.4f,13.4f,15.6f,17.9f,20.4f,23.0f,25.8f,28.8f,31.9f,35.1f,38.6f,42.2f,45.9f,49.8f,53.9f},
    /* 5.0 bar */ {12.1f,14.2f,16.4f,18.9f,21.5f,24.3f,27.2f,30.3f,33.6f,37.1f,40.7f,44.5f,48.4f,52.5f,56.8f},
    /* 5.5 bar */ {12.7f,14.9f,17.2f,19.8f,22.5f,25.5f,28.5f,31.8f,35.2f,38.9f,42.7f,46.6f,50.8f,55.1f,59.6f},
    /* 6.0 bar */ {13.2f,15.5f,18.0f,20.7f,23.5f,26.6f,29.8f,33.2f,36.8f,40.6f,44.6f,48.7f,53.0f,57.5f,62.2f},
};

float calcul_pression_canon(float p_mano_bar,
                             float longueur_tuyau_m,
                             float denivele_m)
{
    float perte_enrouleur = PERTE_ENROULEUR_DEFAULT_BAR;
    float perte_tuyau     = (longueur_tuyau_m / 300.0f) * PERTE_TUYAU_BAR_PAR_300M;
    float perte_denivele  = (denivele_m / 10.0f) * PERTE_DENIVELE_BAR_PAR_10M;

    float p_canon = p_mano_bar - perte_enrouleur - perte_tuyau - perte_denivele;

    if (p_canon < ABAQUE_P_MIN_BAR) {
        ESP_LOGW(TAG, "P_canon=%.2f bar < %.1f bar min (table abaque)", p_canon, ABAQUE_P_MIN_BAR);
    } else if (p_canon > ABAQUE_P_MAX_BAR) {
        ESP_LOGW(TAG, "P_canon=%.2f bar > %.1f bar max (table abaque)", p_canon, ABAQUE_P_MAX_BAR);
    }

    return p_canon;
}

float calcul_debit_m3h(float p_canon_bar, int d_buse_mm)
{
    if (d_buse_mm < ABAQUE_D_MIN_MM || d_buse_mm > ABAQUE_D_MAX_MM) {
        ESP_LOGW(TAG, "Buse %dmm hors table [%d..%d] — clampée",
                 d_buse_mm, ABAQUE_D_MIN_MM, ABAQUE_D_MAX_MM);
        if (d_buse_mm < ABAQUE_D_MIN_MM) d_buse_mm = ABAQUE_D_MIN_MM;
        if (d_buse_mm > ABAQUE_D_MAX_MM) d_buse_mm = ABAQUE_D_MAX_MM;
    }

    // Clamp pression dans les bornes de la table
    float p = p_canon_bar;
    if (p < ABAQUE_P_MIN_BAR) p = ABAQUE_P_MIN_BAR;
    if (p > ABAQUE_P_MAX_BAR) p = ABAQUE_P_MAX_BAR;

    // Index colonne (buse, pas 1mm, pas de fraction nécessaire)
    int id = d_buse_mm - ABAQUE_D_MIN_MM;

    // Index ligne et fraction pour interpolation linéaire sur la pression
    float ip_f = (p - ABAQUE_P_MIN_BAR) / ABAQUE_P_PAS;
    int   ip   = (int)ip_f;
    float fp   = ip_f - (float)ip;

    if (ip >= 4) {
        ip = 4;
        fp = 0.0f;
    }

    float debit = DEBIT_TABLE_M3H[ip][id] * (1.0f - fp);
    if (ip < 4) {
        debit += DEBIT_TABLE_M3H[ip + 1][id] * fp;
    }

    return debit;
}

float calcul_vitesse_cible_m_min(float debit_L_min,
                                  float largeur_m,
                                  float dose_mm)
{
    if (largeur_m <= 0.0f || dose_mm <= 0.0f) {
        ESP_LOGE(TAG, "Paramètres invalides : largeur=%.2fm dose=%.2fmm", largeur_m, dose_mm);
        return 0.0f;
    }
    // dose_mm [L/m²] = débit_L_min / (largeur_m [m] × vitesse [m/min])
    // → vitesse = débit / (largeur × dose)
    return debit_L_min / (largeur_m * dose_mm);
}
