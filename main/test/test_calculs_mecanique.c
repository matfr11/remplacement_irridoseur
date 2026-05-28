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

    ESP_LOGI(TAG, "=== Fin tests mecanique ===");
}
