#include "mosfet_surveillance.h"

void mosfet_surveillance_init(void) {}
bool mosfet_test_demarrage(void) { return true; }

void mosfet_verifier_avant(int pin_ev, bool etat_actuel)
{
    (void)pin_ev; (void)etat_actuel;
}

void mosfet_verifier_apres(int pin_ev, bool nouvel_etat)
{
    (void)pin_ev; (void)nouvel_etat;
}

ev_canal_t mosfet_get_etat(int pin_ev)
{
    (void)pin_ev;
    ev_canal_t c = { MOSFET_OK, MOSFET_OK, false };
    return c;
}

bool mosfet_secours_actif(int pin_ev) { (void)pin_ev; return false; }

const char *mosfet_etat_str(mosfet_etat_t etat)
{
    (void)etat; return "OK";
}

void mosfet_verifier_post_tick(void) {}
void mosfet_reset_etat(void) {}
