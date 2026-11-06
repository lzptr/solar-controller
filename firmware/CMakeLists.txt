cmake_minimum_required(VERSION 3.22)

set(IDF_TARGET "esp32c6")
# The strip preset in CMakePresets.json adds its own defaults file.
if(NOT DEFINED SDKCONFIG_DEFAULTS)
  set(SDKCONFIG_DEFAULTS "sdkconfig.defaults")
endif()

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
idf_build_set_property(MINIMAL_BUILD ON)
project(solar-controller)
