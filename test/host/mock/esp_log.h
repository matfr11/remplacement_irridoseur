#pragma once
#include "mock_log.h"

// Stub ESP-IDF esp_log.h → mock_log
void mock_log_append(const char *tag, char level, const char *fmt, ...);

#define ESP_LOGI(tag, fmt, ...) mock_log_append(tag, 'I', fmt, ##__VA_ARGS__)
#define ESP_LOGE(tag, fmt, ...) mock_log_append(tag, 'E', fmt, ##__VA_ARGS__)
#define ESP_LOGW(tag, fmt, ...) mock_log_append(tag, 'W', fmt, ##__VA_ARGS__)
#define ESP_LOGD(tag, fmt, ...) mock_log_append(tag, 'D', fmt, ##__VA_ARGS__)
