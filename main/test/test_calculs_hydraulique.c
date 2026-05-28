#include "calculs_hydraulique.h"
#include "abaques/abaques.h"
#include "esp_log.h"
#include <math.h>
#include <assert.h>

// Tests unitaires — PR-03
// Compilé uniquement si CONFIG_IRRI_ENABLE_TESTS=y

static const char *TAG = "test_hyd";

#define EPSILON 0.2f

static void assert_near(float attendu, float reel, const char *nom)
{
    if (fabsf(attendu - reel) > EPSILON) {
        ESP_LOGE(TAG, "FAIL %s : attendu=%.3f reçu=%.3f", nom, attendu, reel);
    } else {
        ESP_LOGI(TAG, "PASS %s : %.3f ≈ %.3f", nom, attendu, reel);
    }
}

void test_calculs_hydraulique_run(void)
{
    ESP_LOGI(TAG, "=== Tests calculs hydraulique ===");
    const canon_abaque_t *ab = &ABAQUE_SR150C;

    float debit = 0.0f;

    // entry 0 (p=4.9, buse=17.3mm), dose exacte 25mm → D25=15.3
    float v = lookup_vitesse_cible(ab, 4.9f, 17.3f, 25.0f, &debit);
    assert_near(15.3f, v,     "lookup p=4.9 b=17.3 d=25 vitesse");
    assert_near(23.0f, debit, "lookup p=4.9 b=17.3 d=25 debit");

    // entry 6 (p=7.7, buse=22.9), dose exacte 20mm → D20=28.1
    v = lookup_vitesse_cible(ab, 7.7f, 22.9f, 20.0f, NULL);
    assert_near(28.1f, v, "lookup p=7.7 b=22.9 d=20");

    // entry 11 (p=9.5, buse=20.3), dose exacte 40mm → D40=13.6
    v = lookup_vitesse_cible(ab, 9.5f, 20.3f, 40.0f, NULL);
    assert_near(13.6f, v, "lookup p=9.5 b=20.3 d=40");

    // entry 0, dose 22.5mm — interpolation 25→20 : t=0.5 → 15.3+0.5×3.9=17.25
    v = lookup_vitesse_cible(ab, 4.9f, 17.3f, 22.5f, NULL);
    assert_near(17.25f, v, "lookup interp d=22.5");

    // Clamp dose > 40mm → vitesse_mh[0] (plus lente)
    v = lookup_vitesse_cible(ab, 4.9f, 17.3f, 50.0f, NULL);
    assert_near(9.6f, v, "clamp dose>40mm");

    // Clamp dose < 15mm → vitesse_mh[4] (plus rapide)
    v = lookup_vitesse_cible(ab, 4.9f, 17.3f, 5.0f, NULL);
    assert_near(25.6f, v, "clamp dose<15mm");

    // Nearest neighbor — p/buse légèrement décalés → entry 0
    v = lookup_vitesse_cible(ab, 5.0f, 17.5f, 25.0f, NULL);
    assert_near(15.3f, v, "nearest neighbor");

    ESP_LOGI(TAG, "=== Fin tests hydraulique ===");
}
