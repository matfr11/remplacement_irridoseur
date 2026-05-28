#pragma once

#include "machines/machines.h"

// =============================================================================
// calculs_mecanique.h — Géométrie bobine + étalonnage Irrifrance ESP32
// =============================================================================

/**
 * Rayon effectif à l'étage n (numérotation depuis 1).
 * R_n = r_tambour_vide + (n - 0.5) × d_tuyau_ext
 */
float calcul_rayon_etage(int n, const machine_profile_t *profil);

/**
 * Distance parcourue par impulsion capteur à l'étage donné.
 * dist = (2π × r_etage) / NB_PASTILLES
 */
float calcul_dist_pulse_m(float r_etage_m);

/**
 * Étage courant depuis longueur enroulée.
 * Accumule longueur_etage = spires_par_etage × 2π × R_n jusqu'au bon étage.
 * Retourne 1 au minimum.
 */
int calcul_etage_courant(float longueur_enroulee_m,
                          const machine_profile_t *profil);

/**
 * Longueur totale d'un étage en mètres.
 */
float calcul_longueur_etage_m(int n, const machine_profile_t *profil);

/**
 * Calcule et valide le facteur de correction étalonnage.
 *
 * Conditions de validation :
 *   C1 : nb_impulsions_session > 50
 *   C2 : 0.5 < facteur < 2.0
 *   C3 : |facteur - 1.0| < 0.30
 *
 * @param longueur_theorique_m  Longueur calculée par le firmware
 * @param longueur_reelle_m     Longueur saisie par l'opérateur
 * @param nb_impulsions         Nombre d'impulsions de la session
 * @param facteur_out           Résultat si validation OK
 * @return true si facteur accepté, false sinon
 */
bool calcul_facteur_etalonnage(float longueur_theorique_m,
                                float longueur_reelle_m,
                                int   nb_impulsions,
                                float *facteur_out);
