#pragma once

#include "driver/gpio.h"
#include <stdbool.h>
#include <stdint.h>

// --- Broches GPIO ---
#define PIN_CAPTEUR_VITESSE   GPIO_NUM_34  // Inductif NPN 12V, diviseur 10kΩ/3.3kΩ (input-only)
#define PIN_FIN_COURSE        GPIO_NUM_35  // Fin de course canon (input-only)
#define PIN_SECURITE_SPIRES   GPIO_NUM_32  // Débordement bobine — PRIORITÉ ABSOLUE
#define PIN_CONTACT_POUMON    GPIO_NUM_33  // Poumon plein — capteur indépendant
#define PIN_PRESSOSTAT        GPIO_NUM_27  // Pressostat RP1
#define PIN_EV1               GPIO_NUM_25  // Relais EV1 — vanne canon 12V DC, actif bas
#define PIN_EV2               GPIO_NUM_26  // Relais EV2 — poumon 12V DC, actif bas

// Anti-rebond ISR capteur vitesse (µs)
#define DEBOUNCE_VITESSE_US   5000U
// Timeout vitesse nulle : si aucune impulsion depuis X ms → vitesse = 0
#define TIMEOUT_VITESSE_MS    4000U

// --- Sens des contacteurs ---
// Dépend du type de contact câblé (NO ou NC) avec pull-up 10kΩ vers 3.3V.
//
// Contact NO (Normalement Ouvert) — défaut :
//   Repos : circuit ouvert → GPIO HIGH (pull-up)
//   Activé: circuit fermé  → GPIO LOW  → actif bas
//
// Contact NC (Normalement Fermé) :
//   Repos : circuit fermé  → GPIO LOW
//   Activé: circuit ouvert → GPIO HIGH → actif haut
//
// contact_no_* = true  → contact NO (actif bas)
// contact_no_* = false → contact NC (actif haut)
typedef struct {
    bool contact_no_fin_course;       // Fin de course canon    — défaut true (NO)
    bool contact_no_securite_spires;  // Sécurité spires bobine — défaut true (NO)
    bool contact_no_poumon;           // Contact poumon plein   — défaut true (NO)
    bool contact_no_pressostat;       // Pressostat RP1         — défaut true (NO)
} gpio_sens_t;

// --- État instantané des entrées logiques (après application du sens) ---
typedef struct {
    bool fin_course;       // true = chariot canon arrivé à la bobine
    bool securite_spires;  // true = débordement bobine détecté (DANGER — ARRÊT IMMÉDIAT)
    bool contact_poumon;   // true = poumon en fin de course (plein)
    bool pressostat;       // true = pression présente dans le circuit
} entrees_t;

// --- Données capteur vitesse (partagées entre ISR et tâche) ---
typedef struct {
    volatile uint32_t nb_impulsions;  // Compteur total, incrémenté par ISR
    volatile uint64_t last_isr_us;    // Timestamp dernière impulsion (µs, esp_timer)
    volatile uint32_t periode_us;     // Période mesurée entre deux impulsions consécutives
} capteur_vitesse_t;

// Variable globale accessible par state_machine.c pour le calcul de vitesse
extern capteur_vitesse_t g_capteur_vitesse;

// --- Prototypes ---
void gpio_handler_init(void);

// Applique le sens des contacteurs depuis la config NVS.
// À appeler après config_nvs_lire_machine(), avant le démarrage de la machine d'états.
void gpio_handler_set_sens(const gpio_sens_t *sens);

void gpio_handler_lire_entrees(entrees_t *entrees);
void gpio_ev1_set(bool actif);
void gpio_ev2_set(bool actif);
void gpio_all_ev_off(void);  // Fail-safe : coupe EV1 et EV2 immédiatement
