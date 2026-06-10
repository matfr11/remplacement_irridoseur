#include "tpl5010.h"
#include "gpio_config.h"
#include "driver/gpio.h"
#include "esp_log.h"

static const char *TAG = "tpl5010";

esp_err_t tpl5010_init(void)
{
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << PIN_TPL5010_DONE),
        .mode         = GPIO_MODE_OUTPUT,
        .pull_up_en   = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type    = GPIO_INTR_DISABLE,
    };
    esp_err_t ret = gpio_config(&io_conf);
    if (ret != ESP_OK) return ret;
    gpio_set_level(PIN_TPL5010_DONE, 0);
    ESP_LOGI(TAG, "init OK — DONE GPIO%d, impulsion 2s, timeout ~5.3s (Rext=3.3MΩ)",
             PIN_TPL5010_DONE);
    return ESP_OK;
}

void tpl5010_done_pulse(void)
{
    // Impulsion toutes les 2s (20 ticks × 100ms).
    // Tick N   : DONE passe HIGH
    // Tick N+1 : DONE repasse LOW — donne 100ms de haut, >> 100ns minimum TPL5010
    static uint8_t s_count = 0;
    static bool    s_high  = false;

    if (s_high) {
        gpio_set_level(PIN_TPL5010_DONE, 0);
        s_high = false;
        return;
    }
    if (++s_count < 20) return;
    s_count = 0;
    gpio_set_level(PIN_TPL5010_DONE, 1);
    s_high = true;
}
