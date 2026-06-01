#pragma once

#include "abaques/abaques.h"
#include <stdbool.h>

// =============================================================================
// calculs_hydraulique.h — Vitesse cible + validation programme Irrifrance ESP32
//
// Formule analytique (loi de Torricelli) :
//   Q (m3/h) = k_q * buse_mm^2 * sqrt(p_buse_bar)
//   V (m/h)  = Q * 1000 / (largeur_m * dose_mm)
//
// k_q, k_portee, portee_exp_buse, portee_exp_p, esp_factor : stockes dans l'abaque.
// p_buse : interpole depuis l'abaque (2 voisins IDW sur p_enrouleur x buse_mm).
// largeur_m : espacement entre positions — champ obligatoire (largeur_m < 0.1 -> erreur).
// =============================================================================

/**
 * Warnings non-bloquants retournes par valider_params_programme().
 * Tous les champs false = programme valide.
 */
typedef struct {
    bool pression_basse;        // p_enrouleur < p_min_abaque x 0.75
    bool pression_haute;        // p_enrouleur > p_max_abaque x 1.25
    bool buse_hors_plage;       // buse hors [buse_min x 0.75, buse_max x 1.25]
    bool dose_hors_plage;       // dose hors [15 x 0.75, 40 x 1.25] mm
    bool esp_pos_chevauchement; // espacement < esp_nominal x 0.75 (fort recroisement)
    bool esp_pos_insuf;         // espacement > esp_nominal x 1.10 (risque sous-arrosage)
} hydro_warnings_t;

/**
 * Vitesse cible (m/h) par formule analytique.
 *
 * @param abaque       Abaque actif
 * @param p_enrouleur  Pression manometre (bar)
 * @param buse_mm      Diametre buse (mm)
 * @param dose_mm      Dose cible (mm) — continu, pas de clamping
 * @param largeur_m    Espacement entre positions (m) — OBLIGATOIRE (>0.1), sinon return 0
 * @param debit_out    Debit m3/h (NULL si non souhaite)
 * @param p_buse_out   Pression buse bar (NULL si non souhaite)
 * @return             Vitesse cible m/h (0 si parametre invalide)
 */
float lookup_vitesse_cible(const canon_abaque_t *abaque,
                            float p_enrouleur,
                            float buse_mm,
                            float dose_mm,
                            float largeur_m,
                            float *debit_out,
                            float *p_buse_out);

/**
 * Valide les parametres d'un programme vis-a-vis de l'abaque.
 * Non-bloquant : l'enregistrement peut avoir lieu meme si des warnings sont actifs.
 */
hydro_warnings_t valider_params_programme(const canon_abaque_t *abaque,
                                           float p_enrouleur,
                                           float buse_mm,
                                           float dose_mm,
                                           float largeur_m);

/**
 * Espacement nominal recommande (m) = portee x esp_factor.
 * Portee = rayon du jet (au sens constructeur Irrifrance).
 */
float calcul_esp_nominal_m(const canon_abaque_t *abaque,
                             float p_enrouleur, float buse_mm);

/**
 * Surface arrosee (m2) = longueur x largeur.
 */
float calcul_surface_m2(float longueur_enroulee_m, float largeur_m);

/**
 * Dose instantanee (mm) = debit / (vitesse x largeur) x 1000.
 */
float calcul_dose_inst_mm(float debit_m3h, float vitesse_m_h, float largeur_m);
