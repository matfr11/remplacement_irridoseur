cmake_minimum_required(VERSION 3.16)

# Point d'entrée ESP-IDF — doit être inclus avant project()
include($ENV{IDF_PATH}/tools/cmake/project.cmake)

# Version depuis version.h (source canonique)
file(READ "${CMAKE_SOURCE_DIR}/main/version.h" _VER_H)
string(REGEX MATCH "IRRI_VERSION \"([^\"]+)\"" _ ${_VER_H})
set(PROJECT_VER ${CMAKE_MATCH_1})

# Nommage firmware selon le mode
if(CONFIG_IRRI_PROD)
    if(CONFIG_IRRI_MACHINE_ST1BIS_82_330)
        set(MACHINE_ID "st1bis82330")
    endif()
    if(CONFIG_IRRI_CANON_SR100C)
        set(CANON_ID "sr100c")
    elseif(CONFIG_IRRI_CANON_SR150C)
        set(CANON_ID "sr150c")
    endif()
    set(FIRMWARE_NAME "irrifrance_${MACHINE_ID}_${CANON_ID}_v${PROJECT_VER}")
    message(STATUS "Build PROD : ${FIRMWARE_NAME}.bin")
    message(STATUS "  Version  : ${PROJECT_VER}")
else()
    set(FIRMWARE_NAME "irrifrance_dev")
    message(STATUS "Build DEV  : ${FIRMWARE_NAME}.bin  v${PROJECT_VER}")
endif()

project(${FIRMWARE_NAME})
