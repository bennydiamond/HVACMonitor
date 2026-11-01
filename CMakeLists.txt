cmake_minimum_required(VERSION 3.16.0)

# Disable IDF Component Manager completely so it never downloads registry packages
set(ENV{IDF_COMPONENT_MANAGER} "0")
set(ENV{ESPECTRE_GIT_VERSION} "3.1.0")

list(APPEND EXTRA_COMPONENT_DIRS "${CMAKE_CURRENT_LIST_DIR}/managed_components/espressif__mdns")

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(HVACMonitor)
