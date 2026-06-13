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

// Vérifie l'état logique des deux électrovannes bistables.
// Utilise les getters d'état mémorisé (EVs bistables = pas de courant permanent).
// Nécessite gpio_handler.h dans le fichier appelant.
#define ASSERT_EVS(canon, poumon) do {                                          \
    TEST_ASSERT_EQUAL_INT_MESSAGE((canon)  ? 1 : 0,                             \
        gpio_ev_canon_get()  ? 1 : 0, "etat logique EV canon inattendu");      \
    TEST_ASSERT_EQUAL_INT_MESSAGE((poumon) ? 1 : 0,                             \
        gpio_ev_poumon_get() ? 1 : 0, "etat logique EV poumon inattendu");     \
} while (0)
