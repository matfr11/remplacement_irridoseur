#pragma once

// =============================================================================
// gpio_config.h — Affectation GPIO Irrifrance ESP32
//
// Carte : ESP32 Quad MOS Switch Module
//   QMOS outputs : GPIO 16 (OUT1), 17 (OUT2), 26 (OUT3), 27 (OUT4)
//   LED carte    : GPIO 23
//   Bouton carte : GPIO 0
// =============================================================================

// -----------------------------------------------------------------------------
// SORTIES — Électrovannes bistables (canaux MOSFET carte Quad MOS)
// Commande par impulsion : pulse HIGH pendant DUREE_IMPULSION_EV_MS puis LOW
// OUT1/OUT2 = impulsion OUVRIR, OUT3/OUT4 = impulsion FERMER
// -----------------------------------------------------------------------------

#define PIN_EV_CANON_OUVRIR    16      // QMOS OUT1 — impulsion OUVRIR canon
#define PIN_EV_POUMON_OUVRIR   17      // QMOS OUT2 — impulsion OUVRIR poumon
#define PIN_EV_CANON_FERMER    26      // QMOS OUT3 — impulsion FERMER canon
#define PIN_EV_POUMON_FERMER   27      // QMOS OUT4 — impulsion FERMER poumon

// Durée de l'impulsion de commande EV bistable (ms)
// Valeur terrain à valider — ajustable avant hardcode définitif
#define DUREE_IMPULSION_EV_MS      100
#define DUREE_IMPULSION_EV_MS_MIN   20
#define DUREE_IMPULSION_EV_MS_MAX  500

// -----------------------------------------------------------------------------
// ENTRÉES — Capteurs et contacts
// Contacts NC (Normalement Fermés) — logique : LOW = normal, HIGH = danger/actif
// -----------------------------------------------------------------------------

// Capteur vitesse bobine — diviseur 10kΩ/5.6kΩ (8V → ~2.87V)
// Capteur 2 fils : 2V sans pastille, 8V avec pastille devant le capteur
// Pas de pull-up interne — GPIO input-only
#define PIN_CAPTEUR_VITESSE     34

// Fin de course canon — pull-up externe 10kΩ, contact NC
// LOW = canon déroulé (normal) | HIGH = canon rentré (SEC-1)
#define PIN_FIN_COURSE          35

// Sécurité spires (débordement bobine) — pull-up externe 10kΩ, contact NC
// LOW = normal | HIGH = débordement (SEC-2, priorité absolue)
#define PIN_SECU_SPIRES         32

// Contact poumon plein — pull-up externe 10kΩ, contact NC
// LOW = poumon en cours | HIGH = poumon plein (fin remplissage)
#define PIN_POUMON_PLEIN        33

// Pressostat — pull-up externe 10kΩ, contact NC
// LOW = pression présente (normal) | HIGH = pression absente (pause/attente)
#define PIN_PRESSOSTAT          25

// -----------------------------------------------------------------------------
// Capteur vitesse — paramètres ISR
// -----------------------------------------------------------------------------

// Nombre de pastilles métalliques sur la couronne de la bobine
#define NB_PASTILLES            10

// Anti-rebond temporel ISR — ignorer tout front < 50ms
#define DEBOUNCE_VITESSE_MS     50

// -----------------------------------------------------------------------------
// LED et bouton intégrés à la carte Quad MOS
// -----------------------------------------------------------------------------
#define PIN_LED_CARTE            23     // LED verte de la carte
#define PIN_BOUTON_CARTE          0     // Bouton physique de la carte

// -----------------------------------------------------------------------------
// Heartbeat circuit RC fail-safe (optionnel — activé via Config → Machine)
// Toggle 1Hz → LED carte GPIO 23 + signal circuit RC
// Inactif par défaut (heartbeat_rc_on = false dans config_machine_t)
// -----------------------------------------------------------------------------
#define PIN_HEARTBEAT            23

// -----------------------------------------------------------------------------
// INA3221 — module I2C 3 canaux (tension + courant)
// CH3 = Batterie 12V (seul canal actif — EVs bistables sans surveillance courant)
// -----------------------------------------------------------------------------
#define I2C_SDA_PIN             21
#define I2C_SCL_PIN             22
#define INA3221_I2C_ADDR        0x40   // A0 → GND

#define INA3221_CH_BATTERIE     3

// -----------------------------------------------------------------------------
// Watchdog matériel TPL5010DDCR (optionnel — CONFIG_IRRI_TPL5010)
// DONE → impulsion toutes les 2s depuis state_machine_task()
// RESET → EN ESP32 : reboot si timeout (Rext=3.3MΩ → ~5.3s)
// Note : après reboot, EVs bistables restent dans leur dernier état mécanique
// jusqu'aux impulsions FERMER envoyées par gpio_handler_init() (~2s après boot)
// -----------------------------------------------------------------------------
#define PIN_TPL5010_DONE        13
