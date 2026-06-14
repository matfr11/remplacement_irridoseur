cmake_minimum_required(VERSION 3.16)
project(irrifrance_tests C)
set(CMAKE_C_STANDARD 11)

include(FetchContent)
FetchContent_Declare(unity
  GIT_REPOSITORY https://github.com/ThrowTheSwitch/Unity.git
  GIT_TAG        v2.5.2)
FetchContent_MakeAvailable(unity)

set(PROD_SRCS
  ../../main/calculs_hydraulique.c
  ../../main/calculs_mecanique.c
  ../../main/regulation.c
  ../../main/machines/machines.c
  ../../main/machines/st1bis_82_330.c
  ../../main/abaques/abaques.c
  ../../main/abaques/sr150c.c
  ../../main/abaques/sr100c.c
  ../../main/config_nvs.c
  ../../main/state_machine.c
  ../../main/gpio_handler.c
  ../../main/securites.c
  ../../main/batterie.c
  ../../main/json_utils.c
)

add_executable(irrifrance_tests
  main_test.c
  unity_setup.c
  mock/mock_timer.c
  mock/mock_gpio.c
  mock/mock_nvs.c
  mock/mock_log.c
  mock/mock_telemetry.c
  mock/mock_ina3221.c
  helpers/test_helpers.c
  test_calculs_hydraulique.c
  test_calculs_mecanique.c
  test_regulation.c
  test_state_machine.c
  test_json_utils.c
  test_config_nvs.c
  test_batterie.c
  scenarios/scenario_cycle_normal.c
  scenarios/scenario_perte_pression.c
  scenarios/scenario_urgences.c
  scenarios/scenario_modes_degrades.c
  scenarios/scenario_remplissage_poumon.c
  scenarios/scenario_operateur.c
  scenarios/scenario_arrivee_canon.c
  scenarios/scenario_cycle_poumon.c
  scenarios/scenario_reboot.c
  scenarios/scenario_bilan_session.c
  scenarios/scenario_parametres.c
  scenarios/scenario_commandes_api.c
  scenarios/scenario_batterie.c
  ${PROD_SRCS}
)

target_include_directories(irrifrance_tests PRIVATE
  .
  mock
  helpers
  ../../main
  ../../main/machines
  ../../main/abaques
)

target_compile_definitions(irrifrance_tests PRIVATE
  CONFIG_IRRI_ENABLE_TESTS=1
)

if(WIN32)
  target_link_libraries(irrifrance_tests unity)
else()
  target_link_libraries(irrifrance_tests unity m pthread)
endif()

# Couverture : cmake -B build_cov -DCOVERAGE=ON && cmake --build build_cov
# puis ./build_cov/irrifrance_tests && lcov/genhtml (voir docs/dev/TESTS.md)
option(COVERAGE "Instrumente le build avec gcov" OFF)
if(COVERAGE AND NOT WIN32)
  target_compile_options(irrifrance_tests PRIVATE --coverage -O0 -g)
  target_link_options(irrifrance_tests PRIVATE --coverage)
endif()
