#pragma once

#include <stdint.h>

// --- Table abaque constructeur Irrifrance ---
// Enrouleur : Irrifrance Structure 1 bis — Canon : Nelson SR150
// 13 configurations buse/pression fournies par Irrifrance pour cet ensemble
// Colonnes : p_borne_bar, debit_m3h, buse_mm, largeur_m, portee_m,
//            vitesse_mh[5] pour dose 10/15/20/25/30 mm
#define CANON_N_ENTRIES  13
#define CANON_N_DOSES     5

typedef struct {
    float p_borne_bar;
    float debit_m3h;
    float buse_mm;
    float largeur_m;
    float portee_m;
    float vitesse_mh[CANON_N_DOSES]; // dose 10 / 15 / 20 / 25 / 30 mm
} canon_entry_t;

extern const float         CANON_DOSES_MM[CANON_N_DOSES];
extern const canon_entry_t CANON_TABLE[CANON_N_ENTRIES];

// Pertes de charge par défaut — installation Structure 1 bis (terrain plat, 330m)
#define PERTE_ENROULEUR_DEFAULT_BAR   2.5f
#define PERTE_TUYAU_BAR_PAR_300M      1.0f
#define PERTE_DENIVELE_BAR_PAR_10M    1.0f

// ---
// Pression effective au canon (utilisée pour l'affichage Stats)
float calcul_pression_canon(float p_mano_bar,
                             float longueur_tuyau_m,
                             float denivele_m);

// ---
// Vitesse d'avancement cible à partir de la table constructeur.
// p_borne_bar    : pression mesurée à la borne de l'installation (bar)
// buse_mm        : diamètre buse monté (mm)
// dose_mm        : dose cible (mm = L/m²)
// debit_m3h_out  : débit correspondant à l'entrée sélectionnée (m³/h), peut être NULL
// Retour         : vitesse d'avancement cible (m/h)
float lookup_vitesse_cible(float p_borne_bar, float buse_mm,
                            float dose_mm, float *debit_m3h_out);
