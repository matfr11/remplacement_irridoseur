#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "gpio_config.h"

// =============================================================================
// gpio_handler.h — GPIO, ISR vitesse robuste — Irrifrance ESP32
//
// Contacts NC : LOW = normal, HIGH = danger/actif (fil coupé = danger ✅)
// =============================================================================

// État instantané de toutes les entrées
typedef struct {
    bool fin_course;    // HIGH = canon rentré (SEC-1)
    bool secu_spires;   // HIGH = débordement (SEC-2)
    bool poumon_plein;  // HIGH = poumon plein
    bool pression_ok;   // true = pression présente (pressostat LOW)
} entrees_t;

// -----------------------------------------------------------------------------
// API
// -----------------------------------------------------------------------------
void gpio_handler_init(void);

// Lecture snapshot atomique de toutes les entrées
void gpio_handler_lire_entrees(entrees_t *entrees);

// Sorties électrovannes
void gpio_ev_canon_set(bool actif);
void gpio_ev_poumon_set(bool actif);
void gpio_all_ev_off(void);     // FAIL-SAFE — coupe tout immédiatement

// Vitesse capteur
// Retourne la vitesse calculée sur la fenêtre glissante (m/h), avec facteur.
// Retourne 0.0 si cycles_sans_impulsion > max_cycles_si.
// En mode dégradé A, retourne la vitesse estimée fournie par state_machine.
float gpio_get_vitesse_m_h(float facteur_correction);

// Retourne le compteur d'impulsions brut depuis le dernier reset
uint32_t gpio_get_impulsions(void);

// Remet le compteur d'impulsions à zéro (début de SOUS_REMPLISSAGE)
void gpio_reset_impulsions_cycle(void);

// Incrémente le compteur de cycles sans impulsion (appelé à chaque tick 100ms)
void gpio_handler_tick_cycle(void);

// Mise à jour de la distance par impulsion (calculée par calculs_mecanique)
// À appeler quand l'étage courant change — reçoit calcul_dist_pulse_m(R_etage)
void gpio_handler_set_dist_pulse_m(float dist_pulse_m);

// Paramètres NVS (appelé à l'init depuis config_nvs)
void gpio_handler_set_params(int fenetre_vitesse, int max_cycles_si);

// Mode dégradé A — active/désactive l'estimation de vitesse depuis cycles poumon
void gpio_handler_set_mode_degrade_a(bool actif);

// Vitesse estimée pour le mode dégradé A (fournie par state_machine chaque tick)
// = dist_par_cycle_nvs × cycles_par_min × 60
void gpio_handler_set_vitesse_estimee(float vitesse_m_h);

// -----------------------------------------------------------------------------
// Hooks de test (CONFIG_IRRI_ENABLE_TESTS uniquement)
// Permettent l'injection de timestamps d'impulsion sans matériel
// -----------------------------------------------------------------------------
#ifdef CONFIG_IRRI_ENABLE_TESTS
void gpio_handler_test_injecter_pulse(int64_t timestamp_us);
void gpio_handler_test_reset(void);
#endif
