#pragma once

#include <stdbool.h>

// =============================================================================
// mosfet_surveillance.h — Détection panne MOSFET + basculement secours
// Utilise INA3221 (tension + courant) pour vérifier cohérence GPIO / état EV
// =============================================================================

typedef enum {
    MOSFET_OK,
    MOSFET_GRILLE_CC,       // Court-circuit : tension haute alors que GPIO=LOW
    MOSFET_HS_OUVERT,       // Circuit ouvert : tension nulle alors que GPIO=HIGH
    MOSFET_EV_DEBRANCHEE,   // Courant nul GPIO=HIGH (EV absente ou fil coupé)
} mosfet_etat_t;

typedef struct {
    mosfet_etat_t etat_principal;
    mosfet_etat_t etat_secours;
    bool          secours_actif;
} ev_canal_t;

// Initialise les GPIO relais et OUT3/OUT4. Appeler après gpio_handler_init().
void mosfet_surveillance_init(void);

// Test statique au démarrage (~100ms). Bascule automatiquement si principal CC.
// Retourne false si les deux MOSFETs d'un canal sont défaillants.
bool mosfet_test_demarrage(void);

// Vérification avant commutation — lit INA3221, bascule si incohérence.
void mosfet_verifier_avant(int pin_ev, bool etat_actuel);

// Vérification après commutation — bascule si incohérence.
// Sans délai interne : la stabilisation 20ms est assurée par mosfet_verifier_post_tick().
void mosfet_verifier_apres(int pin_ev, bool nouvel_etat);

// Vérification post-tick des deux canaux (délai 20ms, hors mutex).
// Appeler dans tick_state_machine() APRÈS xSemaphoreGive(s_mutex).
void mosfet_verifier_post_tick(void);

// Remet l'état logique des canaux à {MOSFET_OK, false}.
// Appeler depuis state_machine_cmd_reset() après urgence acquittée.
void mosfet_reset_etat(void);

// Retourne l'état courant d'un canal (PIN_EV_CANON ou PIN_EV_POUMON).
ev_canal_t mosfet_get_etat(int pin_ev);

// Retourne true si le secours est actif sur ce canal.
bool mosfet_secours_actif(int pin_ev);

// Chaîne décrivant l'état du MOSFET (pour JSON/log).
const char *mosfet_etat_str(mosfet_etat_t etat);

// -----------------------------------------------------------------------------
// Hooks de test (CONFIG_IRRI_ENABLE_TESTS uniquement)
// Permettent d'injecter des mesures INA3221 simulées sans matériel
// -----------------------------------------------------------------------------
#ifdef CONFIG_IRRI_ENABLE_TESTS
void mosfet_sim_set_mesure(int pin_ev, float tension_v, float courant_ma);
void mosfet_sim_enable(bool enable);
void mosfet_test_reset(void);
#endif
