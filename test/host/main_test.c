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
void suite_state_machine(void);
void suite_scenario_cycle_normal(void);
void suite_scenario_perte_pression(void);
void suite_scenario_urgences(void);
void suite_scenario_modes_degrades(void);

int main(void)
{
    UNITY_BEGIN();

    suite_calculs_hydraulique();
    suite_calculs_mecanique();
    suite_regulation();
    suite_state_machine();
    suite_scenario_cycle_normal();
    suite_scenario_perte_pression();
    suite_scenario_urgences();
    suite_scenario_modes_degrades();

    return UNITY_END();
}
