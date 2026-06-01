#include "abaques.h"

// Abaque constructeur Nelson SR 150C
// Source : doc Irrifrance "abaque canon pression buse debit.pdf"
// Colonnes : p_enrouleur(bar) | Q(m³/h) | p_buse(bar) | buse(mm) | esp(m)
//          | D40 | D30 | D25 | D20 | D15  [vitesses en m/h pour dose en tête]
// Conversion pouces : 0.68"=17.3 / 0.70"=17.8 / 0.80"=20.3 / 0.90"=22.9 / 1.00"=25.4 mm

const canon_abaque_t ABAQUE_SR150C = {
    .nom              = "SR 150C",
    .constructeur     = "Irrifrance",
    .nb_entrees       = 13,
    .k_q              = 0.039f,
    .k_portee         = 7.06f,
    .portee_exp_buse  = 0.557f,
    .portee_exp_p     = 0.30f,
    .esp_factor       = 1.55f,
    .table = {
     // p_enr   Q       p_buse  buse    esp     D40    D30    D25    D20    D15
        {4.9f,  23.0f,  3.5f,  17.3f,  60.0f,  9.6f, 12.3f, 15.3f, 19.2f, 25.6f},
        {5.6f,  24.6f,  4.0f,  17.3f,  63.0f,  9.8f, 13.0f, 15.6f, 19.5f, 26.0f},
        {5.7f,  29.8f,  3.5f,  20.3f,  63.0f, 11.8f, 15.8f, 18.9f, 23.7f, 31.5f},
        {6.5f,  31.9f,  4.0f,  20.3f,  66.0f, 12.1f, 16.1f, 19.3f, 24.2f, 32.2f},
        {6.8f,  37.8f,  3.5f,  22.9f,  66.0f, 14.3f, 19.1f, 22.9f, 28.6f, 38.2f},
        {6.9f,  27.5f,  5.0f,  17.8f,  66.0f, 10.4f, 13.9f, 16.7f, 20.8f, 27.8f},
        {7.7f,  40.4f,  4.0f,  22.9f,  72.0f, 14.0f, 18.7f, 22.4f, 28.1f, 37.4f},
        {8.0f,  35.7f,  5.0f,  20.3f,  72.0f, 12.4f, 16.5f, 19.8f, 24.8f, 33.1f},
        {8.2f,  30.1f,  6.0f,  17.8f,  66.0f, 11.4f, 15.2f, 18.2f, 22.8f, 30.4f},
        {8.4f,  46.9f,  3.5f,  25.4f,  72.0f, 16.3f, 21.7f, 26.1f, 32.6f, 43.4f},
        {9.4f,  50.1f,  4.0f,  25.4f,  78.0f, 16.1f, 21.4f, 25.7f, 32.1f, 42.8f},
        {9.5f,  39.1f,  6.0f,  20.3f,  72.0f, 13.6f, 18.1f, 21.7f, 27.2f, 36.2f},
        {9.5f,  45.2f,  5.0f,  22.9f,  78.0f, 14.5f, 19.3f, 23.2f, 29.0f, 38.6f},
    },
};
