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

    // Première impulsion immédiate : réarme le timer dès le boot, avant WiFi init.
    // Sans ça, le premier pulse logiciel arrive 2s après le démarrage de state_machine_task.
    // Si le boot prend > 3.3s (WiFi peut prendre 2s), le timer expire → boot loop.
    gpio_set_level(PIN_TPL5010_DONE, 1);
    gpio_set_level(PIN_TPL5010_DONE, 0);

    ESP_LOGI(TAG, "init OK — DONE GPIO%d, periode ~2s, timeout ~5.3s (Rext=3.3MΩ)",
             PIN_TPL5010_DONE);
    return ESP_OK;
}

void tpl5010_done_pulse(void)
{
    // Période réelle : 21 ticks × 100ms ≈ 2.1s (le tick de remise à LOW ne compte pas).
    // Tick N   : DONE passe HIGH
    // Tick N+1 : DONE repasse LOW — 100ms de haut >> 100ns minimum TPL5010
    // Marge de sécurité : 5.3s / 2.1s ≈ 2.5x
    static uint8_t s_count = 0;
    static bool    s_high  = false;

    if (s_high) {
        gpio_set_level(PIN_TPL5010_DONE, 0);
        s_high = false;
        ESP_LOGD(TAG, "DONE pulse envoyé");
        return;
    }
    if (++s_count < 20) return;
    s_count = 0;
    gpio_set_level(PIN_TPL5010_DONE, 1);
    s_high = true;
}
