#pragma once

#include "esp_err.h"

// =============================================================================
// batterie.h — Mesure tension batterie via ADC1 GPIO 36
// Diviseur R1=100kΩ / R2=27kΩ — plage 0..14V → 0..3V
// =============================================================================

// Seuils de tension (V)
#define BATT_V_CHARGE_MIN    13.5f   // Panneau solaire actif
#define BATT_V_PLEINE_MIN    12.4f   // Batterie pleine
#define BATT_V_CORRECTE_MIN  11.8f   // Batterie correcte
#define BATT_V_FAIBLE_MIN    11.5f   // Faible — alerte
#define BATT_V_CRITIQUE_MIN  11.0f   // Critique — alarme

// Diviseur de tension
#define BATT_R1_KOHM         100.0f
#define BATT_R2_KOHM          27.0f
#define BATT_V_MAX            14.0f  // Tension max mesurable

// ADC
#define BATT_ADC_CHANNEL     ADC_CHANNEL_0   // GPIO 36
#define BATT_ADC_NB_SAMPLES  16              // Moyenne anti-bruit

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

// Initialise ADC1 pour la mesure batterie. Appeler dans app_main().
esp_err_t batterie_init(void);

// Lit la tension instantanee (moyenne BATT_ADC_NB_SAMPLES lectures).
float batterie_lire_voltage(void);

// Retourne le statut complet (voltage + etat + pourcentage).
batt_status_t batterie_get_status(void);

// Met a jour les seuils configurables (charge par state_machine depuis NVS).
void batterie_set_seuils(float warn_v, float crit_v);

// Etat sous forme de chaine ASCII.
const char* batterie_etat_str(batt_etat_t etat);

// Couleur CSS associee a l'etat (pour web UI).
const char* batterie_etat_couleur(batt_etat_t etat);
