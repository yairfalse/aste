cmake_minimum_required(VERSION 3.25)
project(aste_power_audit LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
add_compile_options(-Wall -Wextra -Wpedantic -Werror)
option(POWER_SANITIZERS "Audit isolated experiments" OFF)
if(POWER_SANITIZERS)
  add_compile_options(-fsanitize=address,undefined -fno-omit-frame-pointer)
  add_link_options(-fsanitize=address,undefined)
endif()
add_library(power_products
  ../../apps/density/density_processor.cpp ../../apps/harmonic/harmonic_processor.cpp
  ../../apps/sequence/sequence_processor.cpp ../../apps/loop/loop_processor.cpp
  ../../apps/impulse/impulse_processor.cpp ../../apps/field/field_processor.cpp)
target_include_directories(power_products PUBLIC ../..)
include(candidates.cmake)
add_executable(power_lab lab.cpp)
target_link_libraries(power_lab PRIVATE power_products power_candidates)
add_executable(power_partition_probe sequence_partition_probe.cpp)
target_link_libraries(power_partition_probe PRIVATE power_products power_candidates)
execute_process(COMMAND git rev-parse HEAD WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
  OUTPUT_VARIABLE POWER_COMMIT OUTPUT_STRIP_TRAILING_WHITESPACE)
target_compile_definitions(power_lab PRIVATE POWER_COMMIT="${POWER_COMMIT}"
  POWER_COMPILER="${CMAKE_CXX_COMPILER_ID}-${CMAKE_CXX_COMPILER_VERSION}")
enable_testing()
add_test(NAME power_measurement_contract COMMAND power_lab --check)
add_test(NAME power_existing_host_automation COMMAND power_lab --host-check)
