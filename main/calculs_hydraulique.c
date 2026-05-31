#include "calculs_hydraulique.h"
#include "esp_log.h"
#include <math.h>
#include <stddef.h>

static const char *TAG = "calculs_hyd";

// Doses constructeur — ordre DÉCROISSANT (vitesse diminue quand dose augmente)
#define CANON_N_DOSES  5
static const float CANON_DOSES_MM[CANON_N_DOSES] = {40.0f, 30.0f, 25.0f, 20.0f, 15.0f};

// =============================================================================
// Interpolation dose — niveau 2
// Retourne la vitesse interpolée sur la dose pour une entrée donnée.
// Utilise un array local explicite (pas de pointer arithmetic sur struct).
// =============================================================================
static float interpoler_dose(const canon_entry_t *e, float dose_mm)
{
    const float vitesses[CANON_N_DOSES] = {e->D40, e->D30, e->D25, e->D20, e->D15};

    // Clamp haut — dose > 40mm → vitesse minimale (la plus lente)
    if (dose_mm >= CANON_DOSES_MM[0]) {
        return vitesses[0];
    }
    // Clamp bas — dose < 15mm → vitesse maximale (la plus rapide)
    if (dose_mm <= CANON_DOSES_MM[CANON_N_DOSES - 1]) {
        return vitesses[CANON_N_DOSES - 1];
    }
    // Interpolation linéaire entre deux colonnes adjacentes
    for (int d = 0; d < CANON_N_DOSES - 1; d++) {
        if (dose_mm <= CANON_DOSES_MM[d] && dose_mm >= CANON_DOSES_MM[d + 1]) {
            float t = (CANON_DOSES_MM[d] - dose_mm)
                    / (CANON_DOSES_MM[d] - CANON_DOSES_MM[d + 1]);
            return vitesses[d] + t * (vitesses[d + 1] - vitesses[d]);
        }
    }
    return vitesses[0];
}

// =============================================================================
// Distance normalisée (p, buse) — niveau 1
// Ranges calculés dynamiquement depuis l'abaque (robuste pour abaques futurs).
// =============================================================================
static float distance_normalisee(const canon_entry_t *e, float p, float buse,
                                  float p_range, float buse_range)
{
    float dp = (p - e->p_enrouleur) / p_range;
    float db = (buse - e->buse_mm)  / buse_range;
    return sqrtf(dp * dp + db * db);
}

// Calcule les ranges de normalisation depuis l'abaque
static void calc_ranges(const canon_abaque_t *abaque,
                         float *p_range_out, float *buse_range_out)
{
    float p_min = abaque->table[0].p_enrouleur;
    float p_max = p_min;
    float b_min = abaque->table[0].buse_mm;
    float b_max = b_min;

    for (int i = 1; i < abaque->nb_entrees; i++) {
        float p = abaque->table[i].p_enrouleur;
        float b = abaque->table[i].buse_mm;
        if (p < p_min) p_min = p;
        if (p > p_max) p_max = p;
        if (b < b_min) b_min = b;
        if (b > b_max) b_max = b;
    }

    *p_range_out    = (p_max - p_min > 1e-3f) ? (p_max - p_min) : 1.0f;
    *buse_range_out = (b_max - b_min > 1e-3f) ? (b_max - b_min) : 1.0f;
}

// =============================================================================
// Lookup principal — double interpolation
// =============================================================================
float lookup_vitesse_cible(const canon_abaque_t *abaque,
                            float p_enrouleur,
                            float buse_mm,
                            float dose_mm,
                            float largeur_m,
                            float *debit_out,
                            float *p_buse_out)
{
    if (!abaque || abaque->nb_entrees <= 0) {
        ESP_LOGE(TAG, "abaque NULL ou vide");
        if (debit_out)  *debit_out  = 0.0f;
        if (p_buse_out) *p_buse_out = 0.0f;
        return 0.0f;
    }

    if (dose_mm < DOSE_MIN_MM || dose_mm > DOSE_MAX_MM) {
        ESP_LOGW(TAG, "dose %.1fmm hors plage [%.0f-%.0f]",
                 dose_mm, DOSE_MIN_MM, DOSE_MAX_MM);
    }

    float p_range, buse_range;
    calc_ranges(abaque, &p_range, &buse_range);

    // Trouver les 2 entrées les plus proches (nearest neighbor)
    int   idx0 = 0, idx1 = -1;
    float d0   = distance_normalisee(&abaque->table[0], p_enrouleur, buse_mm,
                                     p_range, buse_range);
    float d1   = 1e9f;

    for (int i = 1; i < abaque->nb_entrees; i++) {
        float d = distance_normalisee(&abaque->table[i], p_enrouleur, buse_mm,
                                      p_range, buse_range);
        if (d < d0) {
            d1 = d0; idx1 = idx0;
            d0 = d;  idx0 = i;
        } else if (d < d1) {
            d1 = d; idx1 = i;
        }
    }

    float v0  = interpoler_dose(&abaque->table[idx0], dose_mm);
    float esp = abaque->table[idx0].esp_m;  // esp de référence (défaut = plus proche)

    // Un seul voisin valide ou matchs quasi-identiques → retourner le plus proche
    if (idx1 < 0 || d0 + d1 < 1e-6f) {
        if (debit_out)  *debit_out  = abaque->table[idx0].q_m3h;
        if (p_buse_out) *p_buse_out = abaque->table[idx0].p_buse;
        float larg = (largeur_m > 0.1f) ? largeur_m : esp;
        return v0 * (esp / larg);
    }

    float v1 = interpoler_dose(&abaque->table[idx1], dose_mm);

    // Interpolation pondérée par l'inverse des distances (IDW = interp linéaire pour 2 points)
    float w0 = 1.0f / (d0 + 1e-6f);
    float w1 = 1.0f / (d1 + 1e-6f);
    float wt = w0 + w1;

    float vitesse = (w0 * v0 + w1 * v1) / wt;

    // Interpoler esp_m avec les mêmes poids — largeur de référence au point interpolé
    esp = (w0 * abaque->table[idx0].esp_m + w1 * abaque->table[idx1].esp_m) / wt;

    // Correction largeur : v_cible = v_abaque × (esp_ref / largeur_reelle)
    // largeur_m = 0 → utilise esp_ref (comportement identique à l'ancien code)
    float larg = (largeur_m > 0.1f) ? largeur_m : esp;
    vitesse = vitesse * (esp / larg);

    if (debit_out) {
        *debit_out  = (w0 * abaque->table[idx0].q_m3h  + w1 * abaque->table[idx1].q_m3h)  / wt;
    }
    if (p_buse_out) {
        *p_buse_out = (w0 * abaque->table[idx0].p_buse + w1 * abaque->table[idx1].p_buse) / wt;
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
