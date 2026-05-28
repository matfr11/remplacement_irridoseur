#pragma once

#include <stdint.h>

// Nombre de pastilles métalliques sur la couronne de la bobine
// Valeur mesurée sur machine — constante matérielle
#define NB_PASTILLES  10

// ---
// Rayon effectif à l'étage n (numérotation depuis 1)
// R_n = R_tambour_vide + (n - 0.5) × d_tuyau_ext
// n               : numéro d'étage (1 à nb_etages)
// r_tambour_vide_m: rayon du tambour sans tuyau (m) — paramètre NVS
// d_tuyau_ext_m   : diamètre extérieur du tuyau (m) — paramètre NVS
// Retour          : rayon effectif à l'étage n (m)
float calcul_rayon_etage(int n, float r_tambour_vide_m, float d_tuyau_ext_m);

// ---
// Détermination de l'étage de spires courant depuis le comptage d'impulsions
// nb_impulsions_total: compteur total ISR depuis début de cycle
// r_tambour_vide_m   : rayon tambour nu (m)
// d_tuyau_ext_m      : diamètre ext tuyau (m)
// longueur_tuyau_m   : longueur totale tuyau (m)
// nb_etages          : nombre de couches de spires
// Retour             : numéro d'étage courant (1 à nb_etages)
int calcul_etage_courant(uint32_t nb_impulsions_total,
                          float r_tambour_vide_m,
                          float d_tuyau_ext_m,
                          float longueur_tuyau_m,
                          int   nb_etages);

// ---
// Distance linéaire de tuyau enroulée par impulsion capteur
// dist = (2π × R_etage) / NB_PASTILLES
float calcul_dist_pulse_m(float r_etage_m);

// ---
// Distance linéaire de tuyau enroulée par cycle poumon complet (remplissage + vidange)
// dist = (alpha_deg / 360) × 2π × R_etage
// alpha_deg : angle de rotation de la bobine par cycle poumon (°) — paramètre NVS
float calcul_dist_poumon_m(float r_etage_m, float alpha_deg);

// ---
// Fréquence de cycles poumon nécessaire pour atteindre la vitesse cible
// freq (cycles/min) = vitesse_cible_m_min / dist_poumon_m
// Recalculée à chaque changement d'étage car dist_poumon_m dépend du rayon effectif
float calcul_freq_poumon(float vitesse_cible_m_min, float dist_poumon_m);
