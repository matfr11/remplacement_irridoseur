#include "calculs_mecanique.h"
#include "gpio_config.h"
#include "esp_log.h"
#include <math.h>

static const char *TAG = "calculs_mec";

#ifndef M_PI
#define M_PI 3.14159265358979323846f
#endif

float calcul_rayon_etage(int n, const machine_profile_t *profil)
{
    if (!profil) return 0.0f;
    return profil->r_tambour_vide_m + ((float)n - 0.5f) * profil->d_tuyau_ext_m;
}

float calcul_dist_pulse_m(float r_etage_m)
{
    return (2.0f * (float)M_PI * r_etage_m) / (float)NB_PASTILLES;
}

float calcul_dist_cycle_m(float r_etage_m, float cycles_par_tour)
{
    if (cycles_par_tour <= 0.0f) return 0.0f;
    return (2.0f * (float)M_PI * r_etage_m) / cycles_par_tour;
}

float calcul_longueur_etage_m(int n, const machine_profile_t *profil)
{
    if (!profil) return 0.0f;
    float r = calcul_rayon_etage(n, profil);
    float spires = (n == profil->nb_etages && profil->spires_dernier > 0.0f)
                   ? profil->spires_dernier
                   : profil->spires_par_etage;
    return spires * 2.0f * (float)M_PI * r;
}

int calcul_etage_courant(float longueur_enroulee_m, const machine_profile_t *profil)
{
    if (!profil) return 1;
    float cumul = 0.0f;
    for (int n = 1; n <= profil->nb_etages; n++) {
        cumul += calcul_longueur_etage_m(n, profil);
        if (longueur_enroulee_m <= cumul) {
            return n;
        }
    }
    return profil->nb_etages;
}

bool calcul_facteur_etalonnage(float longueur_theorique_m,
                                float longueur_reelle_m,
                                int   nb_impulsions,
                                float *facteur_out)
{
    if (nb_impulsions < 50) {
        ESP_LOGW(TAG, "Étalonnage refusé — impulsions insuffisantes (%d < 50)", nb_impulsions);
        return false;
    }
    if (longueur_theorique_m < 1.0f) {
        ESP_LOGW(TAG, "Étalonnage refusé — longueur théorique trop faible");
        return false;
    }

    float facteur = longueur_reelle_m / longueur_theorique_m;

    if (facteur < 0.5f || facteur > 2.0f) {
        ESP_LOGW(TAG, "Étalonnage refusé — facteur hors plage [0.5-2.0] : %.3f", facteur);
        return false;
    }
    if (fabsf(facteur - 1.0f) > 0.30f) {
        ESP_LOGW(TAG, "Étalonnage refusé — écart > 30%% (facteur=%.3f)", facteur);
        return false;
    }

    *facteur_out = facteur;
    ESP_LOGI(TAG, "Étalonnage accepté : facteur=%.3f (théo=%.1fm réel=%.1fm)",
             facteur, longueur_theorique_m, longueur_reelle_m);
    return true;
}
