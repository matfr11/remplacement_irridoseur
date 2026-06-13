#include "unity.h"
#include "mock_nvs.h"
#include "mock_timer.h"
#include "mock_gpio.h"
#include "mock_log.h"
#include "state_machine.h"

// Déclarations des suites
void suite_calculs_hydraulique(void);
void suite_calculs_mecanique(void);
void suite_regulation(void);
void suite_json_utils(void);
void suite_config_nvs(void);
void suite_batterie(void);
void suite_state_machine(void);
void suite_scenario_cycle_normal(void);
void suite_scenario_perte_pression(void);
void suite_scenario_urgences(void);
void suite_scenario_modes_degrades(void);
void suite_scenario_remplissage_poumon(void);
void suite_scenario_operateur(void);
void suite_scenario_arrivee_canon(void);
void suite_scenario_cycle_poumon(void);
void suite_scenario_reboot(void);
void suite_scenario_bilan_session(void);
void suite_scenario_parametres(void);
void suite_scenario_commandes_api(void);

int main(void)
{
    UNITY_BEGIN();

    suite_calculs_hydraulique();
    suite_calculs_mecanique();
    suite_regulation();
    suite_json_utils();
    suite_config_nvs();
    suite_batterie();
    suite_state_machine();
    suite_scenario_cycle_normal();
    suite_scenario_perte_pression();
    suite_scenario_urgences();
    suite_scenario_modes_degrades();
    suite_scenario_remplissage_poumon();
    suite_scenario_operateur();
    suite_scenario_arrivee_canon();
    suite_scenario_cycle_poumon();
    suite_scenario_reboot();
    suite_scenario_bilan_session();
    suite_scenario_parametres();
    suite_scenario_commandes_api();

    return UNITY_END();
}
