#pragma once

#include "state_machine.h"

// Types d'alarmes pour la télémétrie distante
typedef enum {
    ALARM_SECURITE_SPIRES = 0,  // Débordement bobine — arrêt immédiat
    ALARM_TIMEOUT_POUMON,       // Poumon n'a pas atteint la fin de course en 60s
    ALARM_ETAT_INTERNE,         // Erreur état machine interne
    ALARM_PRESSION_ABSENTE,     // Pressostat absent alors qu'attendu
} alarm_type_t;

// Couche d'abstraction pour la télémétrie distante.
// Phase 1 : implémentations vides — log série uniquement.
// Phase 2 : brancher module LoRa SX1276/SX1262 sur SPI sans modifier ce code.
//   GPIOs SPI disponibles : MOSI=23, MISO=19, SCK=18, CS=5, RST=14, DIO0=2

void telemetry_send_status(const machine_status_t *status);
void telemetry_send_alarm(alarm_type_t alarm, const char *message);
void telemetry_send_session_end(const session_summary_t *summary);
