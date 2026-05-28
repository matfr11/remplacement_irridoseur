#include "calculs_hydraulique.h"
#include "esp_log.h"
#include <math.h>
#include <stddef.h>

static const char *TAG = "calculs_hyd";

// Doses constructeur — ordre DÉCROISSANT (vitesse diminue quand dose augmente)
#define CANON_N_DOSES  5
static const float CANON_DOSES_MM[CANON_N_DOSES] = {40.0f, 30.0f, 25.0f, 20.0f, 15.0f};

// Plages de normalisation pour le nearest-neighbor (2 lignes les plus proches)
#define CANON_P_RANGE     4.6f   // 9.5 - 4.9 bar
#define CANON_BUSE_RANGE  8.1f   // 25.4 - 17.3 mm

// Retourne la vitesse interpolée sur la dose pour une entrée donnée
static float interpoler_dose(const canon_entry_t *e, float dose_mm)
{
    const float *v = &e->D40;   // D40 est le premier champ vitesse dans la struct

    // Clamp haut — dose > 40mm → vitesse minimale (la plus lente)
    if (dose_mm >= CANON_DOSES_MM[0]) {
        return v[0];
    }
    // Clamp bas — dose < 15mm → vitesse maximale (la plus rapide)
    if (dose_mm <= CANON_DOSES_MM[CANON_N_DOSES - 1]) {
        return v[CANON_N_DOSES - 1];
    }
    // Interpolation linéaire entre deux colonnes adjacentes (ordre décroissant)
    for (int d = 0; d < CANON_N_DOSES - 1; d++) {
        if (dose_mm <= CANON_DOSES_MM[d] && dose_mm >= CANON_DOSES_MM[d + 1]) {
            float t = (CANON_DOSES_MM[d] - dose_mm)
                    / (CANON_DOSES_MM[d] - CANON_DOSES_MM[d + 1]);
            return v[d] + t * (v[d + 1] - v[d]);
        }
    }
    return v[0];  // ne devrait pas arriver
}

// Distance normalisée (p, buse) entre un point cible et une entrée de l'abaque
static float distance_normalisee(const canon_entry_t *e, float p, float buse)
{
    float dp = (p - e->p_enrouleur) / CANON_P_RANGE;
    float db = (buse - e->buse_mm)  / CANON_BUSE_RANGE;
    return sqrtf(dp * dp + db * db);
}

float lookup_vitesse_cible(const canon_abaque_t *abaque,
                            float p_enrouleur,
                            float buse_mm,
                            float dose_mm,
                            float *debit_out)
{
    if (!abaque || abaque->nb_entrees <= 0) {
        ESP_LOGE(TAG, "abaque NULL ou vide");
        if (debit_out) *debit_out = 0.0f;
        return 0.0f;
    }

    if (dose_mm < DOSE_MIN_MM || dose_mm > DOSE_MAX_MM) {
        ESP_LOGW(TAG, "dose %.1fmm hors plage [%.0f-%.0f]",
                 dose_mm, DOSE_MIN_MM, DOSE_MAX_MM);
    }

    // Trouver les 2 entrées les plus proches (nearest neighbor)
    int idx0 = 0, idx1 = -1;
    float d0 = distance_normalisee(&abaque->table[0], p_enrouleur, buse_mm);
    float d1 = 1e9f;

    for (int i = 1; i < abaque->nb_entrees; i++) {
        float d = distance_normalisee(&abaque->table[i], p_enrouleur, buse_mm);
        if (d < d0) {
            d1 = d0; idx1 = idx0;
            d0 = d;  idx0 = i;
        } else if (d < d1) {
            d1 = d; idx1 = i;
        }
    }

    float v0 = interpoler_dose(&abaque->table[idx0], dose_mm);

    if (idx1 < 0 || d0 + d1 < 1e-6f) {
        if (debit_out) *debit_out = abaque->table[idx0].q_m3h;
        return v0;
    }

    float v1 = interpoler_dose(&abaque->table[idx1], dose_mm);

    // Interpolation pondérée par l'inverse des distances
    float w0 = 1.0f / (d0 + 1e-6f);
    float w1 = 1.0f / (d1 + 1e-6f);
    float vitesse = (w0 * v0 + w1 * v1) / (w0 + w1);

    if (debit_out) {
        float q0 = abaque->table[idx0].q_m3h;
        float q1 = abaque->table[idx1].q_m3h;
        *debit_out = (w0 * q0 + w1 * q1) / (w0 + w1);
    }

    return vitesse;
}

float calcul_surface_m2(float longueur_enroulee_m, float largeur_m)
{
    return longueur_enroulee_m * largeur_m;
}

float calcul_dose_inst_mm(float debit_m3h, float vitesse_m_h, float largeur_m)
{
    if (vitesse_m_h < 1e-3f || largeur_m < 1e-3f) {
        return 0.0f;
    }
    return (debit_m3h / (vitesse_m_h * largeur_m)) * 1000.0f;
}
