#include "mock_gpio.h"
#include "gpio_handler.h"
#include "mock_timer.h"

static int s_levels[40] = {0};

int gpio_get_level(gpio_num_t pin)
{
    if (pin < 0 || pin >= 40) return 0;
    return s_levels[pin];
}

void gpio_set_level(gpio_num_t pin, int level)
{
    if (pin < 0 || pin >= 40) return;
    s_levels[pin] = level ? 1 : 0;
}

void mock_gpio_inject_pulses(int n, int64_t interval_us)
{
    int64_t t = esp_timer_get_time();
    for (int i = 0; i < n; i++) {
        t += interval_us;
        gpio_handler_test_injecter_pulse(t);
    }
}

void mock_gpio_reset(void)
{
    for (int i = 0; i < 40; i++) s_levels[i] = 0;
}
