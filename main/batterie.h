#pragma once

#include "esp_err.h"

// =============================================================================
// batterie.h — Mesure tension batterie via INA3221 CH3
// =============================================================================

// Seuils de tension (V)
#define BATT_V_CHARGE_MIN    13.5f   // Panneau solaire actif
#define BATT_V_PLEINE_MIN    12.4f   // Batterie pleine
#define BATT_V_CORRECTE_MIN  11.8f   // Batterie correcte
#define BATT_V_FAIBLE_MIN    11.5f   // Faible — alerte
#define BATT_V_CRITIQUE_MIN  11.0f   // Critique — alarme

#define BATT_V_MAX            14.0f  // Tension max raisonnable

typedef enum {
    BATT_ETAT_CHARGE,    // > 13.5V
    BATT_ETAT_PLEINE,    // 12.4..13.5V
    BATT_ETAT_CORRECTE,  // 11.8..12.4V
    BATT_ETAT_FAIBLE,    // seuil_warn..11.8V
    BATT_ETAT_CRITIQUE,  // < seuil_crit
} batt_etat_t;

typedef struct {
    float       voltage_v;
    batt_etat_t etat;
    int         pourcentage;   // Indicatif 0..100 (11.0V=0%, 12.6V=100%)
} batt_status_t;

// Initialise le module (no-op : INA3221 déjà init par ina3221_init()).
esp_err_t batterie_init(void);

// Lit la tension instantanée via INA3221 CH3.
float batterie_lire_voltage(void);

// Retourne le statut complet (voltage + état + pourcentage).
batt_status_t batterie_get_status(void);

// Met à jour les seuils configurables (chargés depuis NVS par state_machine).
void batterie_set_seuils(float warn_v, float crit_v);

// État sous forme de chaîne ASCII.
const char* batterie_etat_str(batt_etat_t etat);

// Couleur CSS associée à l'état (pour web UI).
const char* batterie_etat_couleur(batt_etat_t etat);

// Simulation (CONFIG_IRRI_TEST_MODE uniquement)
// Injecte une tension fixe ; 0.0 = désactivé, utilise INA3221.
#ifdef CONFIG_IRRI_TEST_MODE
void batterie_sim_set_voltage(float v);
#endif
