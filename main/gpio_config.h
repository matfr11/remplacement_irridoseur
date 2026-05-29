#pragma once

// =============================================================================
// gpio_config.h — Affectation GPIO Irrifrance ESP32
//
// ⚠️  AVANT PR-02 : identifier PIN_EV_CANON et PIN_EV_POUMON
//     depuis le schéma technique de la carte ESP32 Quad MOS Switch.
//     Mettre à jour les deux defines marqués TODO ci-dessous,
//     puis supprimer les lignes #warning.
// =============================================================================

// -----------------------------------------------------------------------------
// SORTIES — Électrovannes
// Carte ESP32 Quad MOS Switch — GPIO à identifier sur schéma technique
// -----------------------------------------------------------------------------

#define PIN_EV_CANON    25      // TODO : identifier canal MOSFET OUT1 sur carte Quad MOS
#define PIN_EV_POUMON   26      // TODO : identifier canal MOSFET OUT2 sur carte Quad MOS

#warning "PIN_EV_CANON et PIN_EV_POUMON sont des valeurs provisoires (GPIO 25/26)."
#warning "Verifier sur le schema de la carte Quad MOS avant toute mise sous tension."

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
#define PIN_PRESSOSTAT          27

// -----------------------------------------------------------------------------
// Capteur vitesse — paramètres ISR
// Déplacé ici depuis calculs_mecanique.h (paramètre hardware, pas calcul)
// -----------------------------------------------------------------------------

// Nombre de pastilles métalliques sur la couronne de la bobine
#define NB_PASTILLES            10

// Anti-rebond temporel ISR — ignorer tout front < 50ms
#define DEBOUNCE_VITESSE_MS     50

// -----------------------------------------------------------------------------
// Mesure tension batterie — ADC1 canal 0, diviseur R1=100kΩ/R2=27kΩ
// -----------------------------------------------------------------------------
#define PIN_BATT_ADC            36
