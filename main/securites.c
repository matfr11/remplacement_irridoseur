#include "securites.h"
#include "gpio_config.h"
#include "gpio_handler.h"
#include "state_machine.h"
#include "driver/gpio.h"
#include "esp_log.h"

static const char *TAG = "securites";

static bool s_bypass_spires = false;

void securites_set_bypass_spires(bool bypass)
{
    s_bypass_spires = bypass;
}

// Implémentation — PR-05
void securites_watchdog(void)
{
    entrees_t e;
    gpio_handler_lire_entrees(&e);

    // SEC-2 : débordement spires — priorité absolue, tout état sans exception
    if (e.secu_spires) {
        if (s_bypass_spires) {
            ESP_LOGW(TAG, "SEC-2 spires active — IGNOREE (mode degrade spires ON)");
        } else {
            gpio_all_ev_off();
            state_machine_declencher_urgence("Debordement bobine");
            return;
        }
    }

    etat_machine_t etat = state_machine_get_etat();

    bool sec1_applicable = (etat == ETAT_OUVERTURE_CANON    ||
                            etat == ETAT_TEMPO_DEPART       ||
                            etat == ETAT_REMPLISSAGE_POUMON ||
                            etat == ETAT_EN_COURS           ||
                            etat == ETAT_PAUSE_PRESSION);

    // SEC-1 : fin de course inattendue (pas en zone de fin normale de bobine)
    // Si longueur restante <= seuil configuré → fin normale, la machine d'états gère
    if (sec1_applicable && e.fin_course && !state_machine_fin_course_est_normale()) {
        gpio_all_ev_off();
        state_machine_declencher_urgence("Fin de course en cours de cycle");
        return;
    }

    // SEC-L : longueur enroulee depasse longueur deroulee + seuil
    // Capteur fin de course defaillant ou etalonnage grossierement incorrect
    if (etat == ETAT_EN_COURS && state_machine_longueur_sec_depassee()) {
        gpio_all_ev_off();
        state_machine_declencher_urgence("Securite longueur - enroule > deroule");
        return;
    }

    // SEC-P pression : gérée dans tick_state_machine()
}
