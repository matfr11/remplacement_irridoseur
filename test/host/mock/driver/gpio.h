#pragma once
#include "../mock_gpio.h"

typedef struct {
    uint64_t pin_bit_mask;
    int      mode;
    int      pull_up_en;
    int      pull_down_en;
    int      intr_type;
} gpio_config_t;

typedef int gpio_int_type_t;

#define GPIO_MODE_INPUT          0
#define GPIO_MODE_OUTPUT         1
#define GPIO_PULLUP_ENABLE       1
#define GPIO_PULLUP_DISABLE      0
#define GPIO_PULLDOWN_DISABLE    0
#define GPIO_INTR_DISABLE        0
#define GPIO_INTR_NEGEDGE        1
#define GPIO_INTR_POSEDGE        2

#define gpio_config(c)                  ((void)(c))
#define gpio_install_isr_service(f)     ((void)(f))
#define gpio_isr_handler_add(p,f,a)     ((void)(p))
#define gpio_set_intr_type(p,t)         ((void)(p))
