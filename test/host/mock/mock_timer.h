#pragma once
#include <stdint.h>

int64_t esp_timer_get_time(void);
void mock_time_advance_ms(int64_t ms);
void mock_time_reset(void);
