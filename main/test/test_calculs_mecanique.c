#include "sdkconfig.h"
#include "calculs_mecanique.h"
#include "machines/machines.h"
#include "esp_log.h"
#include <math.h>

// Tests unitaires — PR-04
static const char *TAG = "test_mec";

#define EPSILON 0.01f

static void assert_near(float attendu, float reel, const char *nom)
{
    if (fabsf(attendu - reel) > EPSILON) {
        ESP_LOGE(TAG, "FAIL %s : attendu=%.4f reçu=%.4f", nom, attendu, reel);
    } else {
        ESP_LOGI(TAG, "PASS %s : %.4f", nom, reel);
    }
}

void test_calculs_mecanique_run(void)
{
    ESP_LOGI(TAG, "=== Tests calculs mecanique ===");

    machine_profile_t profil = MACHINE_ST1BIS_82_330;
    machine_resoudre_double_entree(&profil);

    // Rayon étage 4 : 0.690 + 3.5 × 0.082 = 0.977m
    float r4 = calcul_rayon_etage(4, &profil);
    assert_near(0.977f, r4, "rayon etage 4");

    // Rayon étage 1 : 0.690 + 0.5 × 0.082 = 0.731m
    float r1 = calcul_rayon_etage(1, &profil);
    assert_near(0.731f, r1, "rayon etage 1");

    // largeur_bobine auto-calculée ≈ 13.45 × 0.082 = 1.1029m
    assert_near(1.103f, profil.largeur_bobine_m, "largeur bobine auto");

    // Étage courant à 0m → étage 1
    int e0 = calcul_etage_courant(0.0f, &profil);
    assert_near(1.0f, (float)e0, "etage courant 0m");

    // Validation étalonnage : facteur hors plage → rejet
    float facteur = 0.0f;
    bool ok = calcul_facteur_etalonnage(100.0f, 300.0f, 100, &facteur);
    if (ok) ESP_LOGE(TAG, "FAIL etalonnage facteur 3.0 devrait etre refusé");
    else    ESP_LOGI(TAG, "PASS etalonnage facteur 3.0 refusé");

    // Validation étalonnage : facteur valide 1.05
    ok = calcul_facteur_etalonnage(100.0f, 105.0f, 100, &facteur);
    if (!ok) ESP_LOGE(TAG, "FAIL etalonnage facteur 1.05 devrait etre accepté");
    else     assert_near(1.05f, facteur, "facteur etalonnage 1.05");

    // dist_pulse étage 4 : 2π × 0.977 / 10 ≈ 0.6139m
    float dp4 = calcul_dist_pulse_m(r4);
    assert_near(0.6139f, dp4, "dist pulse etage 4");

    // dist_pulse étage 1 : 2π × 0.731 / 10 ≈ 0.4593m
    float dp1 = calcul_dist_pulse_m(r1);
    assert_near(0.4593f, dp1, "dist pulse etage 1");

    // longueur étage 1 : 13.45 × 2π × 0.731 ≈ 61.78m
    float l1 = calcul_longueur_etage_m(1, &profil);
    assert_near(61.78f, l1, "longueur etage 1");

    // cumul 4 étages ≈ 288.68m (géométrie tambour)
    float l_tot = 0.0f;
    for (int n = 1; n <= 4; n++) l_tot += calcul_longueur_etage_m(n, &profil);
    if (fabsf(l_tot - 288.68f) > 0.1f) {
        ESP_LOGE(TAG, "FAIL longueur totale 4 etages : attendu~288.68 recu=%.2f", l_tot);
    } else {
        ESP_LOGI(TAG, "PASS longueur totale 4 etages : %.2fm", l_tot);
    }

    // rayon étage 5 : 0.690 + 4.5 × 0.082 = 1.059m
    float r5 = calcul_rayon_etage(5, &profil);
    assert_near(1.059f, r5, "rayon etage 5");

    // longueur étage 5 (dernier, 6 spires) : 6 × 2π × 1.059 ≈ 39.92m
    float l5 = calcul_longueur_etage_m(5, &profil);
    if (fabsf(l5 - 39.92f) > 0.1f) {
        ESP_LOGE(TAG, "FAIL longueur etage 5 : attendu~39.92 recu=%.2f", l5);
    } else {
        ESP_LOGI(TAG, "PASS longueur etage 5 (6 spires) : %.2fm", l5);
    }

    // étage courant 70m → étage 2 (L1≈61.78m < 70m ≤ L1+L2≈130.48m)
    int e70 = calcul_etage_courant(70.0f, &profil);
    assert_near(2.0f, (float)e70, "etage courant 70m");

    // étage courant 295m → étage 5 (cumul étages 1-4 = 288.68m)
    int e295 = calcul_etage_courant(295.0f, &profil);
    assert_near(5.0f, (float)e295, "etage courant 295m");

    // étage courant 330m > cumul géo (≈328.6m) → clamp nb_etages = 5
    int e330 = calcul_etage_courant(330.0f, &profil);
    assert_near(5.0f, (float)e330, "etage courant 330m clamp");

    // C1 : impulsions insuffisantes (49 < 50) → refus
    ok = calcul_facteur_etalonnage(100.0f, 105.0f, 49, &facteur);
    if (ok) ESP_LOGE(TAG, "FAIL C1 impulsions 49 devrait etre refusé");
    else    ESP_LOGI(TAG, "PASS C1 impulsions 49 refusé");

    // C2 : facteur < 0.5 (40/100 = 0.4) → refus
    ok = calcul_facteur_etalonnage(100.0f, 40.0f, 100, &facteur);
    if (ok) ESP_LOGE(TAG, "FAIL C2 facteur 0.4 devrait etre refusé");
    else    ESP_LOGI(TAG, "PASS C2 facteur 0.4 refusé");

    // C3 : écart > 30% (135/100 = 1.35, |1.35-1.0|=0.35 > 0.30) → refus
    ok = calcul_facteur_etalonnage(100.0f, 135.0f, 100, &facteur);
    if (ok) ESP_LOGE(TAG, "FAIL C3 ecart 35%% devrait etre refusé");
    else    ESP_LOGI(TAG, "PASS C3 ecart 35%% refusé");

    // C4 : longueur théorique < 1m → refus
    ok = calcul_facteur_etalonnage(0.5f, 0.6f, 100, &facteur);
    if (ok) ESP_LOGE(TAG, "FAIL C4 longueur theo 0.5m devrait etre refusé");
    else    ESP_LOGI(TAG, "PASS C4 longueur theo 0.5m refusé");

    ESP_LOGI(TAG, "=== Fin tests mecanique ===");
}
