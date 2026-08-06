cmake_minimum_required(VERSION 3.25)
project(aste_signal_instruments VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(ASTE_SANITIZERS "Enable AddressSanitizer and UndefinedBehaviorSanitizer" OFF)
option(ASTE_BUILD_VST3 "Build the internal Density VST3 prototype" OFF)

if(ASTE_SANITIZERS AND CMAKE_CXX_COMPILER_ID MATCHES "AppleClang|Clang|GNU")
  add_compile_options(-fsanitize=address,undefined -fno-omit-frame-pointer)
  add_link_options(-fsanitize=address,undefined)
endif()

add_library(density_dsp apps/density/density_processor.cpp)
target_include_directories(density_dsp PUBLIC apps/density)

foreach(target density_dsp)
  target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic -Werror)
endforeach()

add_executable(density_lab apps/dsp-lab/main.cpp)
target_link_libraries(density_lab PRIVATE density_dsp)
target_compile_options(density_lab PRIVATE -Wall -Wextra -Wpedantic -Werror)

enable_testing()
add_executable(density_tests tests/density_tests.cpp)
target_link_libraries(density_tests PRIVATE density_dsp)
target_compile_options(density_tests PRIVATE -Wall -Wextra -Wpedantic -Werror)
add_test(NAME density_tests COMMAND density_tests)
add_test(NAME density_detector_compare
  COMMAND density_lab --detector-compare
          "${CMAKE_CURRENT_BINARY_DIR}/density_detector_compare.csv")
add_test(NAME density_detector_auditions
  COMMAND density_lab --detector-auditions
          "${CMAKE_CURRENT_BINARY_DIR}/density_detector_auditions")
add_test(NAME density_detector_blind
  COMMAND density_lab --detector-blind
          "${CMAKE_CURRENT_BINARY_DIR}/density_detector_blind")

if(ASTE_BUILD_VST3)
  include(FetchContent)
  set(JUCE_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
  set(JUCE_BUILD_EXTRAS OFF CACHE BOOL "" FORCE)
  FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 7c9d3783b127263d72bb65fe0a7e2dc8a02a7ac2 # JUCE 8.0.13
    GIT_SHALLOW TRUE
  )
  FetchContent_MakeAvailable(JUCE)

  juce_add_plugin(DensityD01
    VERSION ${PROJECT_VERSION}
    COMPANY_NAME "Aste Internal"
    BUNDLE_ID "invalid.aste.density-d01"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    COPY_PLUGIN_AFTER_BUILD FALSE
    PLUGIN_MANUFACTURER_CODE Aste
    PLUGIN_CODE Den1
    FORMATS VST3
    VST3_CATEGORIES Fx Dynamics
    PRODUCT_NAME "Density D-01"
  )
  target_sources(DensityD01 PRIVATE apps/density/density_plugin.cpp)
  target_include_directories(DensityD01 PRIVATE apps/density)
  target_link_libraries(DensityD01 PRIVATE density_dsp juce::juce_audio_utils)
  target_compile_definitions(DensityD01 PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
  )
  target_compile_options(DensityD01 PRIVATE -Wall -Wextra -Wpedantic -Werror)
  if(APPLE)
    add_custom_command(TARGET DensityD01_VST3 POST_BUILD
      COMMAND codesign --force --deep --sign - "$<TARGET_BUNDLE_DIR:DensityD01_VST3>"
      COMMENT "Applying final ad-hoc signature after VST3 manifest generation"
      VERBATIM
    )
  endif()

  juce_add_console_app(density_plugin_tests PRODUCT_NAME "Density plugin tests")
  target_sources(density_plugin_tests PRIVATE
    apps/density/density_plugin.cpp
    tests/density_plugin_tests.cpp
  )
  target_include_directories(density_plugin_tests PRIVATE apps/density)
  target_link_libraries(density_plugin_tests PRIVATE density_dsp juce::juce_audio_utils)
  target_compile_definitions(density_plugin_tests PRIVATE
    ASTE_NO_PLUGIN_FACTORY=1
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
  )
  target_compile_options(density_plugin_tests PRIVATE -Wall -Wextra -Wpedantic -Werror)
  add_test(NAME density_plugin_tests COMMAND density_plugin_tests)
  add_test(NAME density_editor_artifacts
    COMMAND density_plugin_tests --editor-artifacts
            "${CMAKE_CURRENT_BINARY_DIR}/density_editor_artifacts")
endif()
