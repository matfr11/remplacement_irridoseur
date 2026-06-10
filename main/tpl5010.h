#pragma once

#include "esp_err.h"

// =============================================================================
// tpl5010.h — Watchdog matériel TPL5010DDCR
//
// Câblage :
//   ESP32 GPIO13  →  TPL5010 DONE
//   TPL5010 RESET →  ESP32 EN  (reboot complet si timeout)
//   Rext = 3,3 MΩ →  timeout ≈ 5,3 s
//
// Utilisation :
//   1. tpl5010_init() depuis app_main(), après gpio_handler_init()
//   2. tpl5010_done_pulse() depuis state_machine_task() à chaque tick (100ms)
//      → impulsion toutes les 2 s, bien sous le timeout de 5,3 s
//
// Si state_machine_task() ne peut plus tourner (deadlock RTOS, exception),
// le TPL5010 active RESET après ~5,3 s → reboot complet de l'ESP32.
// =============================================================================

// Initialise GPIO PIN_TPL5010_DONE en sortie LOW.
esp_err_t tpl5010_init(void);

// À appeler depuis state_machine_task() à chaque tick (100ms).
// Génère une impulsion HIGH/LOW toutes les 2 s.
void tpl5010_done_pulse(void);
