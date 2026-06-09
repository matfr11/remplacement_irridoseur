#include "calculs_hydraulique.h"
#include "esp_log.h"
#include <math.h>
#include <stddef.h>

static const char *TAG = "calculs_hyd";

// =============================================================================
// Distances normalisees (p, buse) — pour selection des 2 entrees les plus proches
// =============================================================================
static float distance_normalisee(const canon_entry_t *e, float p, float buse,
                                  float p_range, float buse_range)
{
    float dp = (p - e->p_enrouleur) / p_range;
    float db = (buse - e->buse_mm)  / buse_range;
    return sqrtf(dp * dp + db * db);
}

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

// Recherche des 2 entrees les plus proches (IDW) — retourne p_buse interpole.
// Ecrit aussi esp_nominal_out si non NULL.
static float interpoler_p_buse(const canon_abaque_t *abaque,
                                 float p_enrouleur, float buse_mm,
                                 float *esp_nominal_out)
{
    float p_range, buse_range;
    calc_ranges(abaque, &p_range, &buse_range);

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

    if (idx1 < 0 || d0 + d1 < 1e-6f) {
        if (esp_nominal_out) {
            float portee = abaque->k_portee
                         * powf(buse_mm, abaque->portee_exp_buse)
                         * powf(abaque->table[idx0].p_buse / 3.5f, abaque->portee_exp_p);
            *esp_nominal_out = portee * abaque->esp_factor;
        }
        return abaque->table[idx0].p_buse;
    }

    float w0 = 1.0f / (d0 + 1e-6f);
    float w1 = 1.0f / (d1 + 1e-6f);
    float wt = w0 + w1;

    float p_buse = (w0 * abaque->table[idx0].p_buse + w1 * abaque->table[idx1].p_buse) / wt;

    if (esp_nominal_out) {
        float portee = abaque->k_portee
                     * powf(buse_mm, abaque->portee_exp_buse)
                     * powf(p_buse / 3.5f, abaque->portee_exp_p);
        *esp_nominal_out = portee * abaque->esp_factor;
    }

    return p_buse;
}

// =============================================================================
// Lookup principal — formule analytique Torricelli
// Q = k_q * buse_mm^2 * sqrt(p_buse)
// V = Q * 1000 / (largeur_m * dose_mm)
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

    if (largeur_m < 0.1f) {
        ESP_LOGE(TAG, "largeur_m non renseignee (%.2f) — champ obligatoire", largeur_m);
        if (debit_out)  *debit_out  = 0.0f;
        if (p_buse_out) *p_buse_out = 0.0f;
        return 0.0f;
    }

    if (dose_mm < 10.0f || dose_mm > 50.0f) {
        ESP_LOGW(TAG, "dose %.1fmm hors plage habituelle [10-50]", dose_mm);
    }

    float p_buse = interpoler_p_buse(abaque, p_enrouleur, buse_mm, NULL);

    float Q = abaque->k_q * buse_mm * buse_mm * sqrtf(p_buse);
    float V = Q * 1000.0f / (largeur_m * dose_mm);

    if (debit_out)  *debit_out  = Q;
    if (p_buse_out) *p_buse_out = p_buse;

    return V;
}

// =============================================================================
// Validation des parametres de programme — warnings non-bloquants
// =============================================================================
hydro_warnings_t valider_params_programme(const canon_abaque_t *abaque,
                                           float p_enrouleur,
                                           float buse_mm,
                                           float dose_mm,
                                           float largeur_m)
{
    hydro_warnings_t w = {0};

    if (!abaque || abaque->nb_entrees <= 0) return w;

    // Bornes abaque pour pression et buse
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

    w.pression_basse = (p_enrouleur < p_min * 0.75f);
    w.pression_haute = (p_enrouleur > p_max * 1.25f);
    w.buse_hors_plage = (buse_mm < b_min * 0.75f || buse_mm > b_max * 1.25f);

    // Bornes dose : D15=15mm / D40=40mm (colonnes extremes de la table constructeur)
    w.dose_hors_plage = (dose_mm < DOSE_MIN_MM * 0.75f || dose_mm > DOSE_MAX_MM * 1.25f);

    // Espacement entre positions vs portee calculee
    if (largeur_m > 0.1f) {
        float esp_nominal = 0.0f;
        interpoler_p_buse(abaque, p_enrouleur, buse_mm, &esp_nominal);
        if (esp_nominal > 0.1f) {
            w.esp_pos_chevauchement = (largeur_m < esp_nominal * 0.75f);
            w.esp_pos_insuf         = (largeur_m > esp_nominal * 1.10f);
        }
    }

    return w;
}

// =============================================================================
// Utilitaires espacement et portee
// =============================================================================
float calcul_esp_nominal_m(const canon_abaque_t *abaque,
                             float p_enrouleur, float buse_mm)
{
    if (!abaque || abaque->nb_entrees <= 0) return 0.0f;
    float esp = 0.0f;
    interpoler_p_buse(abaque, p_enrouleur, buse_mm, &esp);
    return esp;
}

// =============================================================================
// Formules surface et dose instantanee
// =============================================================================
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
