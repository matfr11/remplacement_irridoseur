#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>

// Types ESP-IDF de base
typedef int       esp_err_t;
typedef int       gpio_num_t;
#define ESP_OK    0
#define ESP_FAIL  -1

static inline const char *esp_err_to_name(esp_err_t e) {
    (void)e; return "ERR";
}

// NVS error codes
#define ESP_ERR_NVS_NO_FREE_PAGES      (-2)
#define ESP_ERR_NVS_NEW_VERSION_FOUND  (-3)
#define ESP_ERR_NVS_NOT_FOUND          (-4)
#define ESP_ERR_INVALID_ARG            (-5)

// NVS types
typedef void *nvs_handle_t;
typedef enum { NVS_READONLY = 0, NVS_READWRITE = 1 } nvs_open_mode_t;

// gpio_num_t aliases ESP-IDF
#define GPIO_NUM_NC   -1
