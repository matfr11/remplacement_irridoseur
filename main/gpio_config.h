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
// SORTIES — Électrovannes (canaux MOSFET carte Quad MOS)
// -----------------------------------------------------------------------------

#define PIN_EV_CANON    16      // QMOS OUT1
#define PIN_EV_POUMON   17      // QMOS OUT2
// GPIO 26 = QMOS OUT3 — non utilisé
// GPIO 27 = QMOS OUT4 — non utilisé

// -----------------------------------------------------------------------------
// ENTRÉES — Capteurs et contacts
// Contacts NC (Normalement Fermés) — logique : LOW = normal, HIGH = danger/actif
// -----------------------------------------------------------------------------

// Capteur vitesse bobine — diviseur 10kΩ/3.3kΩ (12V → ~3V)
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
// MOSFETs secours — carte Quad MOS OUT3/OUT4
// -----------------------------------------------------------------------------
#define PIN_MOSFET_SECOURS_CANON   26   // QMOS OUT3
#define PIN_MOSFET_SECOURS_POUMON  27   // QMOS OUT4

// -----------------------------------------------------------------------------
// Relais de basculement MOSFET — HIGH level trigger
// LOW = repos = COM→NC = MOSFET principal
// HIGH = actif = COM→NO = MOSFET secours
// -----------------------------------------------------------------------------
#define PIN_RELAIS_CANON    2
#define PIN_RELAIS_POUMON   4

// -----------------------------------------------------------------------------
// INA3221 — module I2C 3 canaux (tension + courant)
// CH1 = EV_CANON, CH2 = EV_POUMON, CH3 = Batterie 12V
// -----------------------------------------------------------------------------
#define I2C_SDA_PIN             21
#define I2C_SCL_PIN             22
#define INA3221_I2C_ADDR        0x40   // A0 → GND

#define INA3221_CH_EV_CANON     1
#define INA3221_CH_EV_POUMON    2
#define INA3221_CH_BATTERIE     3

// Seuils détection MOSFET
#define SEUIL_TENSION_EV_V      6.0f   // > 6V = EV alimentée
#define SEUIL_COURANT_EV_MA    50.0f   // < 50mA = circuit suspect
