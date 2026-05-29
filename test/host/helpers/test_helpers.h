#pragma once
#include "config_nvs.h"
#include "state_machine.h"

// Configure NVS avec un programme valide (dose=20, largeur=18, buse=25, p=3.5)
void config_set_programme_valide(void);

// Configure NVS avec un programme invalide (dose=0)
void config_set_programme_invalide(void);

// Avance la machine vers un état cible en appelant tick_state_machine()
// (max 500 ticks). Renvoie le nombre de ticks effectués, -1 si état non atteint.
int aller_a_etat(etat_machine_t etat_cible, int max_ticks);
