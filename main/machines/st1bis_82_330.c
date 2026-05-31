#include "machines.h"

// Irrifrance Structure 1 bis — tuyau PE Ø82mm ext / 330m
// Source données : tableau constructeur Irrifrance ST.1 Bis
// r_tambour_vide calculé : R_ext_max - nb_etages × d_tuyau_ext
//   = 0.977 - 4 × 0.082 × (3.5/4) ... méthode approx = 0.977 - 3.5 × 0.082 = 0.690m
// Note : largeur_bobine_m calculée auto → spires_par_etage × d_tuyau_ext = 13.45 × 0.082 ≈ 1.103m
// ⚠️  t_vidange_s à mesurer sur le terrain avant mise en service

const machine_profile_t MACHINE_ST1BIS_82_330 = {
    .nom               = "ST1 Bis " "\xc3\x98" "82-330m",
    .constructeur      = "Irrifrance",
    .longueur_tuyau_m  = 330.0f,
    .d_tuyau_ext_m     = 0.082f,
    .r_tambour_vide_m  = 0.690f,
    .nb_etages         = 4,
    .abaque_idx        = 0,
    .largeur_bobine_m  = 0.0f,      // calculée auto depuis spires_par_etage
    .spires_par_etage  = 13.45f,
    .cycles_par_tour   = 40.0f,     // mesure physique : 40 cycles poumon par tour de bobine
    .t_vidange_s       = 0.0f,      // ⚠️ À mesurer terrain
    .facteur_correction = 1.0f,
};
