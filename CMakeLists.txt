cmake_minimum_required(VERSION 3.16)

# Point d'entrée ESP-IDF — doit être inclus avant project()
include($ENV{IDF_PATH}/tools/cmake/project.cmake)

# Version firmware — surcharger via ./build.sh <version> ou idf.py -DPROJECT_VER="x.y.z" build
if(NOT DEFINED PROJECT_VER)
    set(PROJECT_VER "1.0.0")
endif()

project(irrifrance-esp32)
