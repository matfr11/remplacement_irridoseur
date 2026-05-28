#include "securites.h"
#include "gpio_config.h"
#include "gpio_handler.h"
#include "state_machine.h"
#include "driver/gpio.h"

// Implémentation — PR-05
void securites_watchdog(void)
{
    entrees_t e;
    gpio_handler_lire_entrees(&e);

    // SEC-2 : débordement spires — priorité absolue, tout état sans exception
    if (e.secu_spires) {
        gpio_all_ev_off();
        state_machine_declencher_urgence("Debordement bobine");
        return;
    }

    etat_machine_t etat = state_machine_get_etat();

    bool sec1_applicable = (etat == ETAT_OUVERTURE_CANON    ||
                            etat == ETAT_TEMPO_DEPART       ||
                            etat == ETAT_REMPLISSAGE_POUMON ||
                            etat == ETAT_EN_COURS           ||
                            etat == ETAT_PAUSE_PRESSION);

    // SEC-1 : fin de course en cours de cycle (non applicable TEMPO_ARRIVEE, ARRET_*)
    if (sec1_applicable && e.fin_course) {
        gpio_all_ev_off();
        state_machine_declencher_urgence("Fin de course en cours de cycle");
        return;
    }

    // SEC-P pression : gérée dans tick_state_machine()
}
