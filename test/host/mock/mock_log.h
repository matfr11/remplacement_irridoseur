#pragma once
#include <stdbool.h>

// Redirige ESP_LOGI/E/W vers un buffer interne capturé
bool mock_log_contains(const char *substr);
void mock_log_reset(void);
