#include "test_helpers.h"
#include "config_nvs.h"
#include "state_machine.h"
#include "mock_timer.h"

void config_set_programme_valide(void)
{
    config_nvs_init();

    config_machine_t m = CFG_MACHINE_DEFAUT;
    m.machine_active  = 0;
    m.t_vidange_s     = 5.0f;
    m.kp_regulation   = 0.1f;
    m.n_cycles_calib  = 3;
    m.fenetre_vitesse = 5;
    m.max_cycles_si   = 15;
    config_nvs_sauver_machine(&m);

    config_programme_t p = {
        .nom             = "Test",
        .dose_mm         = 20.0f,
        .largeur_m       = 18.0f,
        .buse_mm         = 25,
        .pression_bar    = 3.5f,
        .tempo_depart_on  = false,
        .tempo_depart_s   = 0,
        .tempo_arrivee_on = false,
        .tempo_arrivee_s  = 0,
    };
    config_nvs_sauver_programme(0, &p);
    config_nvs_sauver_prog_actif(0);
}

void config_set_programme_invalide(void)
{
    config_nvs_init();

    config_machine_t m = CFG_MACHINE_DEFAUT;
    config_nvs_sauver_machine(&m);

    config_programme_t p = {
        .nom          = "Invalide",
        .dose_mm      = 0.0f,   // invalide
        .largeur_m    = 0.0f,
        .buse_mm      = 0,
        .pression_bar = 0.0f,
    };
    config_nvs_sauver_programme(0, &p);
    config_nvs_sauver_prog_actif(0);
}

int aller_a_etat(etat_machine_t etat_cible, int max_ticks)
{
    for (int i = 0; i < max_ticks; i++) {
        if (state_machine_get_etat() == etat_cible) return i;
        tick_state_machine();
        mock_time_advance_ms(100);
    }
    if (state_machine_get_etat() == etat_cible) return max_ticks;
    return -1;
}
