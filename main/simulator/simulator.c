#include "sdkconfig.h"
#include "simulator.h"

#ifdef CONFIG_IRRI_TEST_MODE

#include "gpio_handler.h"
#include "gpio_config.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_timer.h"
#include <string.h>

static const char *TAG = "simulator";

#define SIM_GPIO_MAX  40

static int   s_gpio_values[SIM_GPIO_MAX];
static float s_vitesse_auto_mh = 0.0f;

int sim_gpio_get_level(gpio_num_t pin)
{
    if (pin < 0 || pin >= SIM_GPIO_MAX) return 0;
    return s_gpio_values[pin];
}

void sim_gpio_set_level(gpio_num_t pin, int level)
{
    if (pin < 0 || pin >= SIM_GPIO_MAX) return;
    s_gpio_values[pin] = level ? 1 : 0;
    ESP_LOGI(TAG, "GPIO %d → %d", pin, s_gpio_values[pin]);
}

// Tâche de génération automatique de pulses vitesse
static void task_pulses(void *arg)
{
    (void)arg;
    while (1) {
        float v = s_vitesse_auto_mh;
        if (v > 0.0f) {
            // dist_pulse ≈ 0.61m (étage 4) — approximation pour simulateur
            const float DIST_PULSE_M = 0.61f;
            // v m/h → pulses/s = (v/3600) / dist_pulse
            float pps = (v / 3600.0f) / DIST_PULSE_M;
            if (pps > 0.01f) {
                int64_t interval_us = (int64_t)(1e6f / pps);
                vTaskDelay(pdMS_TO_TICKS(interval_us / 1000));
                gpio_handler_test_injecter_pulse(esp_timer_get_time());
                continue;
            }
        }
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

void sim_set_vitesse_auto(float v_m_h)
{
    s_vitesse_auto_mh = v_m_h;
}

void simulator_init(void)
{
    memset(s_gpio_values, 0, sizeof(s_gpio_values));
    // NC par défaut : contacts normalement fermés = 0 = normal
    s_gpio_values[PIN_PRESSOSTAT]   = 0;  // pression présente
    s_gpio_values[PIN_FIN_COURSE]   = 0;  // pas en bout
    s_gpio_values[PIN_SECU_SPIRES]  = 0;  // normal
    s_gpio_values[PIN_POUMON_PLEIN] = 0;  // pas plein

    xTaskCreate(task_pulses, "sim_pulses", 2048, NULL, 5, NULL);
    ESP_LOGI(TAG, "Simulateur initialisé");
}

#endif // CONFIG_IRRI_TEST_MODE
