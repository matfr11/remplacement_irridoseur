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

// Vérifie l'état physique des deux électrovannes (niveaux GPIO réels).
// Ajouté après la revue 2026-06 : les scénarios validaient les transitions
// d'états sans jamais vérifier les sorties physiques correspondantes.
// Nécessite mock_gpio.h + gpio_config.h dans le fichier appelant.
#define ASSERT_EVS(canon, poumon) do {                                          \
    TEST_ASSERT_EQUAL_INT_MESSAGE((canon)  ? 1 : 0,                             \
        gpio_get_level(PIN_EV_CANON),  "niveau GPIO EV canon inattendu");       \
    TEST_ASSERT_EQUAL_INT_MESSAGE((poumon) ? 1 : 0,                             \
        gpio_get_level(PIN_EV_POUMON), "niveau GPIO EV poumon inattendu");      \
} while (0)
