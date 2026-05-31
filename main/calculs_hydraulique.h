#pragma once

#include "abaques/abaques.h"

// =============================================================================
// calculs_hydraulique.h — Lookup vitesse cible + débit Irrifrance ESP32
//
// Double interpolation :
//   Niveau 1 — Ligne (p_enrouleur + buse_mm) : 2 voisins les plus proches
//   Niveau 2 — Dose : interpolation linéaire entre colonnes D40/D30/D25/D20/D15
//
// Doses {40,30,25,20,15} mm — ordre DÉCROISSANT
// Vitesse DIMINUE quand dose AUGMENTE
// =============================================================================

// Plage doses valides (mm)
#define DOSE_MIN_MM   10.0f
#define DOSE_MAX_MM   50.0f

/**
 * Lookup vitesse cible — double interpolation sur l'abaque.
 *
 * L'abaque donne les vitesses pour l'espacement de référence esp_m.
 * La correction largeur_m ramène la vitesse à la largeur réelle configurée :
 *   v_cible = v_abaque × (esp_interpolé / largeur_m)
 *
 * @param abaque       Pointeur abaque actif
 * @param p_enrouleur  Pression manomètre (bar)
 * @param buse_mm      Diamètre buse (mm)
 * @param dose_mm      Dose cible mm — plage 10-50mm, extrapolation hors plage
 * @param largeur_m    Largeur position arrosée (m). 0 = utilise esp_m de l'abaque (compat)
 * @param debit_out    Débit résultant m³/h (NULL si non souhaité)
 * @param p_buse_out   Pression effective à la buse bar (NULL si non souhaité)
 * @return             Vitesse cible m/h (> 0 garanti si abaque valide)
 */
float lookup_vitesse_cible(const canon_abaque_t *abaque,
                            float p_enrouleur,
                            float buse_mm,
                            float dose_mm,
                            float largeur_m,
                            float *debit_out,
                            float *p_buse_out);

/**
 * Calcule la surface arrosée en m².
 * surface = longueur_enroulee_m × largeur_m
 */
float calcul_surface_m2(float longueur_enroulee_m, float largeur_m);

/**
 * Calcule la dose instantanée en mm.
 * dose = (debit_m3h / (vitesse_m_h × largeur_m)) × 1000
 */
float calcul_dose_inst_mm(float debit_m3h, float vitesse_m_h, float largeur_m);
