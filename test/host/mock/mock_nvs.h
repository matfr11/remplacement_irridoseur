#pragma once
#include "esp_idf_stubs.h"
#include "esp_system.h"
#include <stddef.h>

// Stub NVS sur RAM — implémenté dans mock_nvs.c
esp_err_t nvs_flash_init(void);
esp_err_t nvs_flash_erase(void);
esp_err_t nvs_open(const char *namespace_name, nvs_open_mode_t open_mode, nvs_handle_t *out_handle);
void      nvs_close(nvs_handle_t handle);
esp_err_t nvs_commit(nvs_handle_t handle);
esp_err_t nvs_get_i32(nvs_handle_t handle, const char *key, int32_t *out_value);
esp_err_t nvs_set_i32(nvs_handle_t handle, const char *key, int32_t value);
esp_err_t nvs_get_u32(nvs_handle_t handle, const char *key, uint32_t *out_value);
esp_err_t nvs_set_u32(nvs_handle_t handle, const char *key, uint32_t value);
esp_err_t nvs_get_i8(nvs_handle_t handle, const char *key, int8_t *out_value);
esp_err_t nvs_set_i8(nvs_handle_t handle, const char *key, int8_t value);
esp_err_t nvs_get_u8(nvs_handle_t handle, const char *key, uint8_t *out_value);
esp_err_t nvs_set_u8(nvs_handle_t handle, const char *key, uint8_t value);
esp_err_t nvs_get_str(nvs_handle_t handle, const char *key, char *out_value, size_t *length);
esp_err_t nvs_set_str(nvs_handle_t handle, const char *key, const char *value);
esp_err_t nvs_get_blob(nvs_handle_t handle, const char *key, void *out_value, size_t *length);
esp_err_t nvs_set_blob(nvs_handle_t handle, const char *key, const void *value, size_t length);
esp_err_t nvs_erase_key(nvs_handle_t handle, const char *key);
esp_err_t nvs_erase_all(nvs_handle_t handle);

// Réinitialise tout le stockage RAM (entre tests)
void mock_nvs_reset(void);

// Contrôle la raison de reboot retournée par esp_reset_reason() (défaut ESP_RST_EXT)
void mock_set_reset_reason(esp_reset_reason_t r);
