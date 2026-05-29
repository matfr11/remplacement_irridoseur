#include "mock_timer.h"

static int64_t s_mock_us = 0;

int64_t esp_timer_get_time(void) { return s_mock_us; }
void mock_time_advance_ms(int64_t ms) { s_mock_us += ms * 1000; }
void mock_time_reset(void) { s_mock_us = 0; }
