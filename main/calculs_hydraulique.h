#pragma once

#include <stdint.h>

// --- Table débit canon (m³/h) ---
// Lignes  : pression au canon — 4.0 / 4.5 / 5.0 / 5.5 / 6.0 bar (5 lignes)
// Colonnes: diamètre buse     — 12 à 26 mm, pas 1mm (15 colonnes)
// Source  : UASA46, "Choisir le bon diamètre de buse", 19/06/2025
extern const float DEBIT_TABLE_M3H[5][15];

// Bornes de la table abaque
#define ABAQUE_P_MIN_BAR    4.0f
#define ABAQUE_P_MAX_BAR    6.0f
#define ABAQUE_P_PAS        0.5f
#define ABAQUE_D_MIN_MM     12
#define ABAQUE_D_MAX_MM     26

// Pertes de charge par défaut — installation Structure 1 bis (terrain plat, 330m)
#define PERTE_ENROULEUR_DEFAULT_BAR   2.5f   // Médiane fourchette 2-3 bar (UASA46)
#define PERTE_TUYAU_BAR_PAR_300M      1.0f   // Perte tuyau PE 82mm / 300m

// Facteur dénivelé : 1 bar pour 10m de montée (positif = canon plus haut que bobine)
#define PERTE_DENIVELE_BAR_PAR_10M    1.0f

// ---
// Calcul pression effective au canon
// p_mano_bar      : pression lue au manomètre (avant circuit enrouleur)
// longueur_tuyau_m: longueur tuyau PE (m), typiquement 330m sur réf.
// denivele_m      : dénivelé bobine→canon (m), 0 = terrain plat
// Retour          : pression effective au canon (bar)
float calcul_pression_canon(float p_mano_bar,
                             float longueur_tuyau_m,
                             float denivele_m);

// ---
// Débit canon par interpolation dans la table abaque UASA46
// p_canon_bar: pression effective au canon (bar)
// d_buse_mm  : diamètre buse (mm, entier de 12 à 26)
// Retour     : débit (m³/h)
// Note       : warning log si hors bornes, valeur clampée
float calcul_debit_m3h(float p_canon_bar, int d_buse_mm);

// ---
// Vitesse d'avancement cible du chariot canon
// dose_mm = débit_L_min / (largeur_m × vitesse_m_min)  → vitesse = débit / (largeur × dose)
// debit_L_min : débit calculé (L/min)
// largeur_m   : largeur de position d'arrosage (m)
// dose_mm     : dose cible (mm = L/m²)
// Retour      : vitesse linéaire cible (m/min)
float calcul_vitesse_cible_m_min(float debit_L_min,
                                  float largeur_m,
                                  float dose_mm);
