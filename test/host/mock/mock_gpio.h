#pragma once
#include <stdint.h>
#include "esp_idf_stubs.h"

int  gpio_get_level(gpio_num_t pin);
void gpio_set_level(gpio_num_t pin, int level);

// Injecte N impulsions espacées de interval_us microsecondes
void mock_gpio_inject_pulses(int n, int64_t interval_us);

// Remet tous les niveaux à 0
void mock_gpio_reset(void);
