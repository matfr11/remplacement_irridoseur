#pragma once

#include "sdkconfig.h"

#ifdef CONFIG_IRRI_TEST_MODE

#include <stdbool.h>
#include "driver/gpio.h"

// Remplace gpio_get_level() dans gpio_handler.c via macro READ_GPIO
int sim_gpio_get_level(gpio_num_t pin);

// Injecte une valeur GPIO (0 ou 1) depuis le WebSocket
void sim_gpio_set_level(gpio_num_t pin, int level);

// Active/désactive la génération automatique de pulses vitesse
void sim_set_vitesse_auto(float v_m_h);

// Initialise le simulateur (crée la tâche pulses si v_auto > 0)
void simulator_init(void);

#endif // CONFIG_IRRI_TEST_MODE
