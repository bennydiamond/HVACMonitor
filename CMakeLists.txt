cmake_minimum_required(VERSION 3.16.0)

# Disable IDF Component Manager completely so it never downloads registry packages
set(ENV{IDF_COMPONENT_MANAGER} "0")

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(HVACMonitor)