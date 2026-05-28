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
float gpio_get_vitesse_m_h(float facteur_correction);

// Retourne le compteur d'impulsions brut depuis le dernier reset
uint32_t gpio_get_impulsions(void);

// Remet le compteur d'impulsions à zéro (début de SOUS_REMPLISSAGE)
void gpio_reset_impulsions_cycle(void);

// Incrémente le compteur de cycles sans impulsion (appelé à chaque tick 100ms)
void gpio_handler_tick_cycle(void);
