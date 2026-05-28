#pragma once

#include <stdbool.h>

// =============================================================================
// regulation.h — Feedforward + correction Kp Irrifrance ESP32
// =============================================================================

/**
 * Calcule T_attente du cycle poumon.
 *
 * T_attente = T_cycle_cible - t_remplissage_mesure - t_vidange
 *
 * Guards :
 *   T_attente < 0    → 0 + alerte pression insuffisante
 *   T_attente > 300s → log warning anomalie
 *
 * @param dist_par_cycle_m       Distance par cycle auto-calibrée (m)
 * @param vitesse_cible_m_s      Vitesse cible m/s
 * @param t_remplissage_mesure_s Durée remplissage mesuré (s)
 * @param t_vidange_s            Durée vidange mécanique (s)
 * @param alerte_pression_out    true si T_attente était négatif
 * @return T_attente en secondes (>= 0)
 */
float calcul_t_attente_s(float dist_par_cycle_m,
                          float vitesse_cible_m_s,
                          float t_remplissage_mesure_s,
                          float t_vidange_s,
                          bool  *alerte_pression_out);

/**
 * Correction proportionnelle vitesse réelle vs cible.
 * Activée après n_cycles_calib cycles.
 *
 * @param t_attente_s         T_attente calculé par feedforward
 * @param vitesse_reelle_m_h  Vitesse mesurée
 * @param vitesse_cible_m_h   Vitesse cible
 * @param kp                  Gain (NVS, défaut 0.1)
 * @return T_attente corrigé
 */
float correction_vitesse(float t_attente_s,
                          float vitesse_reelle_m_h,
                          float vitesse_cible_m_h,
                          float kp);

/**
 * Met à jour la moyenne glissante dist_par_cycle.
 * Utilise un buffer circulaire de 5 valeurs.
 *
 * @param nouvelle_dist_m    Distance mesurée sur ce cycle
 * @return Moyenne courante sur les dernières valeurs
 */
float regulation_update_dist_par_cycle(float nouvelle_dist_m);

/**
 * Remet à zéro le buffer de calibration dist_par_cycle.
 * Appelé au début de chaque session.
 */
void regulation_reset_calibration(void);

/**
 * Nombre de cycles de calibration effectués.
 */
int regulation_get_nb_cycles(void);
