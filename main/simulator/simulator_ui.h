#pragma once

#ifdef CONFIG_IRRI_TEST_MODE

// Symboles générés par EMBED_FILES CMakeLists.txt
// Fichier: simulator/simulator_ui.html → _binary_simulator_ui_html_start/end
extern const uint8_t simulator_ui_html_start[] asm("_binary_simulator_ui_html_start");
extern const uint8_t simulator_ui_html_end[]   asm("_binary_simulator_ui_html_end");

#endif // CONFIG_IRRI_TEST_MODE
