#include "machines.h"

// Irrifrance Structure 1 bis — tuyau PE Ø82mm ext / 330m
// Source : tableau constructeur ST.1 Bis + mesures terrain
//
// Géométrie vérifiée (st1bis_82x330.md) :
//   r_tambour_vide = 0.690m  (calculé : R_étage4=0.977m − 3.5×0.082)
//   5 étages : 4 complets (13.45 spires) + 1 partiel (6 spires terrain)
//   Vérification : 61.8+68.7+75.6+82.6+39.9 = 328.6m + 1.4m résidu = 330m ✅
//   Résidu absorbé par facteur_correction NVS
// ⚠️  t_vidange_s à mesurer sur le terrain avant mise en service

const machine_profile_t MACHINE_ST1BIS_82_330 = {
    .nom               = "ST1 Bis " "\xc3\x98" "82-330m",
    .constructeur      = "Irrifrance",
    .longueur_tuyau_m  = 330.0f,
    .d_tuyau_ext_m     = 0.082f,    // Ø EXTÉRIEUR (épaisseur 6mm → Ø int = 70mm)
    .r_tambour_vide_m  = 0.690f,    // Calculé depuis L_spire_étage4 = 6.14m
    .nb_etages         = 5,         // 4 étages complets + 1 partiel (mesuré terrain)
    .abaque_idx        = 1,         // SR 100C (idx 1 dans ABAQUES_LISTE)
    .largeur_bobine_m  = 0.0f,      // Calculée auto : 13.45 × 0.082 ≈ 1.103m
    .spires_par_etage  = 13.45f,    // Étages 1-4 (tableau constructeur ST.1 Bis)
    .spires_dernier    = 6.0f,      // Étage 5 — mesuré terrain
    .cycles_par_tour   = 40.0f,     // Mesure physique : 40 cycles poumon par tour
    .t_vidange_s       = 2.0f,      // ⚠️ À affiner terrain
    .facteur_correction = 1.0f,
};
