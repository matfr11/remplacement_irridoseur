#pragma once

#include <stdint.h>

// Nombre de pastilles métalliques sur la couronne de la bobine (valeur matérielle)
#define NB_PASTILLES  10

// ---
// Rayon effectif à l'étage n (numérotation depuis 1)
// R_n = R_tambour_vide + (n - 0.5) × d_tuyau_ext
float calcul_rayon_etage(int n, float r_tambour_vide_m, float d_tuyau_ext_m);

// ---
// Détermine l'étage de spires courant depuis le comptage d'impulsions ISR
int calcul_etage_courant(uint32_t nb_impulsions_total,
                          float r_tambour_vide_m,
                          float d_tuyau_ext_m,
                          float longueur_tuyau_m,
                          int   nb_etages);

// ---
// Distance linéaire parcourue par impulsion capteur : 2π×R / NB_PASTILLES
float calcul_dist_pulse_m(float r_etage_m);

// ---
// Longueur de tuyau correspondant à un comptage d'impulsions (tient compte des étages)
float calcul_longueur_depuis_impulsions(uint32_t nb_impulsions,
                                         float r_tambour_vide_m,
                                         float d_tuyau_ext_m,
                                         float longueur_tuyau_m,
                                         int   nb_etages);

// ---
// Facteur de correction capteur vitesse (calibration terrain)
// k = longueur_reelle_m / longueur_calculee_m
// Retourne k si valide — l'appelant doit sauvegarder en NVS.
// Retourne -1.0f si rejet — NE PAS écrire le NVS, conserver le facteur précédent.
// Rejeté si : longueur_calculee < 5m, ou k hors [0.5 ; 2.0], ou longueur_reelle <= 0.
float calcul_correction_capteur(float longueur_reelle_m, float longueur_calculee_m);

// ---
// Moyenne glissante de la distance parcourue par cycle poumon (fenêtre 5 cycles)
// Appelée en fin de phase VIDANGE avec le résultat mesuré par le capteur.
void  update_dist_par_cycle(float nouvelle_mesure_m);
float get_dist_par_cycle_m(void);

// ---
// Calcul T_attente par feedforward
// T_attente = dist_par_cycle / vitesse_cible_m_s - t_remplissage - t_vidange
// Retourne 0.0f si vitesse max insuffisante ou paramètres invalides.
float calcul_t_attente_s(float dist_par_cycle_m,
                          float vitesse_cible_m_s,
                          float t_remplissage_mesure_s,
                          float t_vidange_s);

// ---
// Correction proportionnelle de T_attente depuis la vitesse réelle mesurée.
// S'applique après N cycles d'auto-calibration.
// erreur = vitesse_reelle_m_h - vitesse_cible_m_h
// T_nouveau = T_attente_actuel + erreur × kp  (clampé à 0 si négatif)
float correction_vitesse(float t_attente_actuel_s,
                          float vitesse_reelle_m_h,
                          float vitesse_cible_m_h,
                          float kp);
