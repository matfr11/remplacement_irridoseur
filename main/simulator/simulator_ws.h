#pragma once

#include "sdkconfig.h"

#ifdef CONFIG_IRRI_TEST_MODE

#include "esp_http_server.h"

// Enregistre les routes /test (HTML) et /ws_test (WebSocket simulateur)
void simulator_ws_register(httpd_handle_t server);

#endif // CONFIG_IRRI_TEST_MODE
