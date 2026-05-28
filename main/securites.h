#pragma once

#include <stdbool.h>
#include "state_machine.h"

// =============================================================================
// securites.h — Watchdog sécurités Irrifrance ESP32
//
// SEC-2 (spires) : priorité absolue, tout état
// SEC-1 (fin course) : hors TEMPO_ARRIVEE et ARRET_*
// SEC-P (pression) : géré dans state_machine.c (pause / reprise)
// =============================================================================

// Appelé EN PREMIER dans state_machine_task — priorité absolue.
// Lit l'état courant via state_machine_get_etat(), pilote les GPIO,
// appelle state_machine_declencher_urgence() si nécessaire.
void securites_watchdog(void);
