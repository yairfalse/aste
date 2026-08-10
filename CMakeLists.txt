cmake_minimum_required(VERSION 3.25)
project(aste_signal_instruments VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(ASTE_SANITIZERS "Enable AddressSanitizer and UndefinedBehaviorSanitizer" OFF)
option(ASTE_BUILD_VST3 "Build the internal VST3 prototypes" OFF)
set(ASTE_JUCE_COMMIT "7c9d3783b127263d72bb65fe0a7e2dc8a02a7ac2")

find_package(Git QUIET)
set(ASTE_GIT_COMMIT "unknown")
set(ASTE_GIT_COMMIT_TIMESTAMP "unknown")
set(ASTE_SOURCE_DIRTY "unknown")
if(Git_FOUND)
  execute_process(
    COMMAND "${GIT_EXECUTABLE}" rev-parse --verify HEAD
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
    RESULT_VARIABLE ASTE_GIT_RESULT
    OUTPUT_VARIABLE ASTE_GIT_COMMIT
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET)
  if(NOT ASTE_GIT_RESULT EQUAL 0)
    set(ASTE_GIT_COMMIT "unknown")
  endif()
  execute_process(
    COMMAND "${GIT_EXECUTABLE}" show -s --format=%ct HEAD
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
    RESULT_VARIABLE ASTE_GIT_TIMESTAMP_RESULT
    OUTPUT_VARIABLE ASTE_GIT_COMMIT_TIMESTAMP
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET)
  if(NOT ASTE_GIT_TIMESTAMP_RESULT EQUAL 0)
    set(ASTE_GIT_COMMIT_TIMESTAMP "unknown")
  endif()
  execute_process(
    COMMAND "${GIT_EXECUTABLE}" status --porcelain
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
    OUTPUT_VARIABLE ASTE_GIT_STATUS
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET)
  if(ASTE_GIT_STATUS STREQUAL "")
    set(ASTE_SOURCE_DIRTY "false")
  else()
    set(ASTE_SOURCE_DIRTY "true")
  endif()
endif()
if(CMAKE_OSX_ARCHITECTURES)
  set(ASTE_BUILD_ARCHITECTURE "${CMAKE_OSX_ARCHITECTURES}")
else()
  set(ASTE_BUILD_ARCHITECTURE "${CMAKE_SYSTEM_PROCESSOR}")
endif()
configure_file(cmake/build-metadata.json.in
               "${CMAKE_CURRENT_BINARY_DIR}/build-metadata.json" @ONLY)

if(ASTE_SANITIZERS AND CMAKE_CXX_COMPILER_ID MATCHES "AppleClang|Clang|GNU")
  add_compile_options(-fsanitize=address,undefined -fno-omit-frame-pointer)
  add_link_options(-fsanitize=address,undefined)
endif()

add_library(density_dsp apps/density/density_processor.cpp)
target_include_directories(density_dsp PUBLIC apps/density libs/parameters)

add_library(harmonic_dsp apps/harmonic/harmonic_processor.cpp)
target_include_directories(harmonic_dsp PUBLIC apps/harmonic libs/parameters)

add_library(sequence_dsp apps/sequence/sequence_processor.cpp)
target_include_directories(sequence_dsp PUBLIC apps/sequence libs/parameters)

add_library(loop_dsp apps/loop/loop_processor.cpp)
target_include_directories(loop_dsp PUBLIC apps/loop libs/parameters)

add_library(impulse_dsp apps/impulse/impulse_processor.cpp)
target_include_directories(impulse_dsp PUBLIC apps/impulse libs/parameters)

add_library(field_dsp apps/field/field_processor.cpp)
target_include_directories(field_dsp PUBLIC apps/field libs/parameters)

if(CMAKE_CXX_COMPILER_ID MATCHES "AppleClang|Clang")
  add_custom_target(density_static_analysis
    COMMAND "${CMAKE_CXX_COMPILER}" --analyze
            -Xanalyzer -analyzer-output=text
            -std=c++20 -Wall -Wextra -Wpedantic -Werror
            -I"${CMAKE_CURRENT_SOURCE_DIR}/apps/density"
            "${CMAKE_CURRENT_SOURCE_DIR}/apps/density/density_processor.cpp"
    COMMENT "Running Clang static analysis on Density DSP"
    VERBATIM)
  add_custom_target(harmonic_static_analysis
    COMMAND "${CMAKE_CXX_COMPILER}" --analyze
            -Xanalyzer -analyzer-output=text
            -std=c++20 -Wall -Wextra -Wpedantic -Werror
            -I"${CMAKE_CURRENT_SOURCE_DIR}/apps/harmonic"
            "${CMAKE_CURRENT_SOURCE_DIR}/apps/harmonic/harmonic_processor.cpp"
    COMMENT "Running Clang static analysis on Harmonic DSP"
    VERBATIM)
  add_custom_target(sequence_static_analysis
    COMMAND "${CMAKE_CXX_COMPILER}" --analyze
            -Xanalyzer -analyzer-output=text
            -std=c++20 -Wall -Wextra -Wpedantic -Werror
            -I"${CMAKE_CURRENT_SOURCE_DIR}/apps/sequence"
            "${CMAKE_CURRENT_SOURCE_DIR}/apps/sequence/sequence_processor.cpp"
    COMMENT "Running Clang static analysis on Sequence DSP"
    VERBATIM)
  add_custom_target(loop_static_analysis
    COMMAND "${CMAKE_CXX_COMPILER}" --analyze
            -Xanalyzer -analyzer-output=text
            -std=c++20 -Wall -Wextra -Wpedantic -Werror
            -I"${CMAKE_CURRENT_SOURCE_DIR}/apps/loop"
            "${CMAKE_CURRENT_SOURCE_DIR}/apps/loop/loop_processor.cpp"
    COMMENT "Running Clang static analysis on Loop DSP"
    VERBATIM)
  add_custom_target(impulse_static_analysis
    COMMAND "${CMAKE_CXX_COMPILER}" --analyze
            -Xanalyzer -analyzer-output=text
            -std=c++20 -Wall -Wextra -Wpedantic -Werror
            -I"${CMAKE_CURRENT_SOURCE_DIR}/apps/impulse"
            "${CMAKE_CURRENT_SOURCE_DIR}/apps/impulse/impulse_processor.cpp"
    COMMENT "Running Clang static analysis on Impulse DSP"
    VERBATIM)
  add_custom_target(field_static_analysis
    COMMAND "${CMAKE_CXX_COMPILER}" --analyze
            -Xanalyzer -analyzer-output=text
            -std=c++20 -Wall -Wextra -Wpedantic -Werror
            -I"${CMAKE_CURRENT_SOURCE_DIR}/apps/field"
            "${CMAKE_CURRENT_SOURCE_DIR}/apps/field/field_processor.cpp"
    COMMENT "Running Clang static analysis on Field DSP"
    VERBATIM)
endif()

foreach(target density_dsp harmonic_dsp sequence_dsp loop_dsp impulse_dsp
               field_dsp)
  target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic -Werror)
endforeach()

add_executable(density_lab apps/dsp-lab/main.cpp)
target_link_libraries(density_lab PRIVATE density_dsp)
target_compile_options(density_lab PRIVATE -Wall -Wextra -Wpedantic -Werror)

add_executable(harmonic_lab apps/dsp-lab/harmonic.cpp)
target_link_libraries(harmonic_lab PRIVATE harmonic_dsp)
target_compile_options(harmonic_lab PRIVATE -Wall -Wextra -Wpedantic -Werror)

add_executable(sequence_lab apps/dsp-lab/sequence.cpp)
target_link_libraries(sequence_lab PRIVATE sequence_dsp)
target_compile_options(sequence_lab PRIVATE -Wall -Wextra -Wpedantic -Werror)

add_executable(loop_lab apps/dsp-lab/loop.cpp)
target_link_libraries(loop_lab PRIVATE loop_dsp)
target_compile_options(loop_lab PRIVATE -Wall -Wextra -Wpedantic -Werror)

add_executable(impulse_lab apps/dsp-lab/impulse.cpp)
target_link_libraries(impulse_lab PRIVATE impulse_dsp)
target_compile_options(impulse_lab PRIVATE -Wall -Wextra -Wpedantic -Werror)

add_executable(field_lab apps/dsp-lab/field.cpp)
target_link_libraries(field_lab PRIVATE field_dsp)
target_compile_options(field_lab PRIVATE -Wall -Wextra -Wpedantic -Werror)

enable_testing()
find_package(Python3 COMPONENTS Interpreter)
if(Python3_Interpreter_FOUND)
  add_test(NAME documentation_links
    COMMAND Python3::Interpreter "${CMAKE_CURRENT_SOURCE_DIR}/tools/check_docs.py"
            "${CMAKE_CURRENT_SOURCE_DIR}")
  add_test(NAME build_metadata
    COMMAND Python3::Interpreter
            "${CMAKE_CURRENT_SOURCE_DIR}/tools/check_build_metadata.py"
            "${CMAKE_CURRENT_BINARY_DIR}/build-metadata.json")
  add_test(NAME dependency_security
    COMMAND Python3::Interpreter
            "${CMAKE_CURRENT_SOURCE_DIR}/tools/check_dependency_security.py"
            "${CMAKE_CURRENT_SOURCE_DIR}/docs/security/dependency-audit.json"
            "${CMAKE_CURRENT_SOURCE_DIR}")
endif()
add_executable(density_tests tests/density_tests.cpp)
target_link_libraries(density_tests PRIVATE density_dsp)
target_compile_options(density_tests PRIVATE -Wall -Wextra -Wpedantic -Werror)
add_test(NAME density_tests COMMAND density_tests)
add_executable(harmonic_tests tests/harmonic_tests.cpp)
target_link_libraries(harmonic_tests PRIVATE harmonic_dsp)
target_compile_options(harmonic_tests PRIVATE -Wall -Wextra -Wpedantic -Werror)
add_test(NAME harmonic_tests COMMAND harmonic_tests)
add_executable(sequence_tests tests/sequence_tests.cpp)
target_link_libraries(sequence_tests PRIVATE sequence_dsp)
target_compile_options(sequence_tests PRIVATE -Wall -Wextra -Wpedantic -Werror)
add_test(NAME sequence_tests COMMAND sequence_tests)
add_executable(loop_tests tests/loop_tests.cpp)
target_link_libraries(loop_tests PRIVATE loop_dsp)
target_compile_options(loop_tests PRIVATE -Wall -Wextra -Wpedantic -Werror)
add_test(NAME loop_tests COMMAND loop_tests)
add_executable(impulse_tests tests/impulse_tests.cpp)
target_link_libraries(impulse_tests PRIVATE impulse_dsp)
target_compile_options(impulse_tests PRIVATE -Wall -Wextra -Wpedantic -Werror)
add_test(NAME impulse_tests COMMAND impulse_tests)
add_executable(field_tests tests/field_tests.cpp)
target_link_libraries(field_tests PRIVATE field_dsp)
target_compile_options(field_tests PRIVATE -Wall -Wextra -Wpedantic -Werror)
add_test(NAME field_tests COMMAND field_tests)
if(APPLE)
  add_test(NAME density_installer_syntax
    COMMAND /bin/sh -n
            "${CMAKE_CURRENT_SOURCE_DIR}/tools/install_density_macos.sh")
  add_test(NAME harmonic_installer_syntax
    COMMAND /bin/sh -n
            "${CMAKE_CURRENT_SOURCE_DIR}/tools/install_harmonic_macos.sh")
  add_test(NAME sequence_installer_syntax
    COMMAND /bin/sh -n
            "${CMAKE_CURRENT_SOURCE_DIR}/tools/install_sequence_macos.sh")
  add_test(NAME loop_installer_syntax
    COMMAND /bin/sh -n
            "${CMAKE_CURRENT_SOURCE_DIR}/tools/install_loop_macos.sh")
  add_test(NAME impulse_installer_syntax
    COMMAND /bin/sh -n
            "${CMAKE_CURRENT_SOURCE_DIR}/tools/install_impulse_macos.sh")
  add_test(NAME field_installer_syntax
    COMMAND /bin/sh -n
            "${CMAKE_CURRENT_SOURCE_DIR}/tools/install_field_macos.sh")
  add_test(NAME build_and_install_syntax
    COMMAND /bin/sh -n
            "${CMAKE_CURRENT_SOURCE_DIR}/tools/build_and_install_macos.sh")
endif()
add_test(NAME harmonic_reference_compare
  COMMAND harmonic_lab --compare
          "${CMAKE_CURRENT_BINARY_DIR}/harmonic-reference-compare.csv")
add_test(NAME harmonic_preemphasis_compare
  COMMAND harmonic_lab --preemphasis
          "${CMAKE_CURRENT_BINARY_DIR}/harmonic-preemphasis-tone.csv"
          "${CMAKE_CURRENT_BINARY_DIR}/harmonic-preemphasis-imd.csv")
add_test(NAME harmonic_stateful_compare
  COMMAND harmonic_lab --stateful
          "${CMAKE_CURRENT_BINARY_DIR}/harmonic-stateful-tone.csv"
          "${CMAKE_CURRENT_BINARY_DIR}/harmonic-stateful-imd.csv"
          "${CMAKE_CURRENT_BINARY_DIR}/harmonic-stateful-dynamics.csv")
add_test(NAME harmonic_product_report
  COMMAND harmonic_lab --product-report
          "${CMAKE_CURRENT_BINARY_DIR}/harmonic-product-report.csv")
add_test(NAME sequence_product_render
  COMMAND sequence_lab "${CMAKE_CURRENT_BINARY_DIR}/sequence-product-render.csv")
add_test(NAME loop_product_render
  COMMAND loop_lab "${CMAKE_CURRENT_BINARY_DIR}/loop-product-render.csv")
add_test(NAME impulse_product_render
  COMMAND impulse_lab "${CMAKE_CURRENT_BINARY_DIR}/impulse-product-render.csv")
add_test(NAME field_product_render
  COMMAND field_lab "${CMAKE_CURRENT_BINARY_DIR}/field-product-render.csv")
add_test(NAME density_detector_compare
  COMMAND density_lab --detector-compare
          "${CMAKE_CURRENT_BINARY_DIR}/density_detector_compare.csv")
add_test(NAME density_detector_auditions
  COMMAND density_lab --detector-auditions
          "${CMAKE_CURRENT_BINARY_DIR}/density_detector_auditions")
add_test(NAME density_detector_blind
  COMMAND density_lab --detector-blind
          "${CMAKE_CURRENT_BINARY_DIR}/density_detector_blind")
add_test(NAME density_production_golden
  COMMAND density_lab --golden
          "${CMAKE_CURRENT_BINARY_DIR}/density_golden"
          "${CMAKE_CURRENT_SOURCE_DIR}/tests/golden/density-production-v1.csv")
add_test(NAME density_production_consistency
  COMMAND density_lab --production-consistency
          "${CMAKE_CURRENT_BINARY_DIR}/density-production-consistency.csv")
add_test(NAME density_nonlinear_alias
  COMMAND density_lab --alias-report
          "${CMAKE_CURRENT_BINARY_DIR}/density-nonlinear-alias.csv")
add_test(NAME density_oversampling_reference
  COMMAND density_lab --oversampling-report
          "${CMAKE_CURRENT_BINARY_DIR}/density-oversampling-reference.csv")
add_test(NAME density_oversampling_prototype
  COMMAND density_lab --oversampling-prototype
          "${CMAKE_CURRENT_BINARY_DIR}/density-oversampling-prototype.csv")
add_test(NAME density_halfband_prototype
  COMMAND density_lab --halfband-prototype
          "${CMAKE_CURRENT_BINARY_DIR}/density-halfband-prototype.csv")
add_test(NAME density_kaiser_prototype
  COMMAND density_lab --kaiser-prototype
          "${CMAKE_CURRENT_BINARY_DIR}/density-kaiser-prototype.csv")
add_test(NAME density_kaiser_sweep
  COMMAND density_lab --kaiser-sweep
          "${CMAKE_CURRENT_BINARY_DIR}/density-kaiser-sweep.csv")
add_test(NAME density_kaiser_rate_sweep
  COMMAND density_lab --kaiser-rate-sweep
          "${CMAKE_CURRENT_BINARY_DIR}/density-kaiser-rate-sweep.csv")
add_test(NAME density_kaiser_linear
  COMMAND density_lab --kaiser-linear-report
          "${CMAKE_CURRENT_BINARY_DIR}/density-kaiser-linear.csv")
add_test(NAME density_kaiser_length
  COMMAND density_lab --kaiser-length-report
          "${CMAKE_CURRENT_BINARY_DIR}/density-kaiser-length.csv")
add_test(NAME density_kaiser_finalists
  COMMAND density_lab --kaiser-finalist-report
          "${CMAKE_CURRENT_BINARY_DIR}/density-kaiser-finalists.csv")
add_test(NAME density_oversampled_chain
  COMMAND density_lab --oversampled-chain-report
          "${CMAKE_CURRENT_BINARY_DIR}/density-oversampled-chain.csv")
add_test(NAME density_oversampling_auditions
  COMMAND density_lab --oversampling-auditions
          "${CMAKE_CURRENT_BINARY_DIR}/density-oversampling-auditions")
add_test(NAME density_automation
  COMMAND density_lab --automation-report
          "${CMAKE_CURRENT_BINARY_DIR}/density-automation.csv")
add_test(NAME density_output_smoothing
  COMMAND density_lab --output-smoothing-report
          "${CMAKE_CURRENT_BINARY_DIR}/density-output-smoothing.csv")
add_test(NAME density_drive_smoothing
  COMMAND density_lab --drive-smoothing-report
          "${CMAKE_CURRENT_BINARY_DIR}/density-drive-smoothing.csv")
add_test(NAME density_attack_smoothing
  COMMAND density_lab --attack-smoothing-report
          "${CMAKE_CURRENT_BINARY_DIR}/density-attack-smoothing.csv")
add_test(NAME density_blend_smoothing
  COMMAND density_lab --blend-smoothing-report
          "${CMAKE_CURRENT_BINARY_DIR}/density-blend-smoothing.csv")
add_test(NAME density_automation_auditions
  COMMAND density_lab --automation-auditions
          "${CMAKE_CURRENT_BINARY_DIR}/density-automation-auditions")
add_test(NAME density_stereo_stability
  COMMAND density_lab --stereo-stability-report
          "${CMAKE_CURRENT_BINARY_DIR}/density-stereo-stability.csv")
add_test(NAME density_stereo_stability_auditions
  COMMAND density_lab --stereo-stability-auditions
          "${CMAKE_CURRENT_BINARY_DIR}/density-stereo-stability-auditions")
add_test(NAME density_macro_auditions
  COMMAND density_lab --density-macro-auditions
          "${CMAKE_CURRENT_BINARY_DIR}/density-macro-auditions")
if(ASTE_SANITIZERS)
  set_tests_properties(
    density_oversampling_prototype density_halfband_prototype PROPERTIES
    ENVIRONMENT "ASTE_SKIP_BENCHMARK=1")
  set_tests_properties(
    density_kaiser_sweep density_kaiser_rate_sweep density_kaiser_linear
    density_kaiser_finalists PROPERTIES
    ENVIRONMENT "ASTE_FAST_RESEARCH=1")
  set_tests_properties(density_kaiser_length PROPERTIES
    ENVIRONMENT "ASTE_FAST_RESEARCH=1;ASTE_SKIP_BENCHMARK=1")
  set_tests_properties(density_oversampled_chain PROPERTIES
    ENVIRONMENT "ASTE_SKIP_BENCHMARK=1")
endif()

if(ASTE_BUILD_VST3)
  include(FetchContent)
  include(CheckCXXCompilerFlag)
  check_cxx_compiler_flag(
    "-Wno-unnecessary-virtual-specifier"
    ASTE_HAS_NO_UNNECESSARY_VIRTUAL_SPECIFIER)
  set(JUCE_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
  set(JUCE_BUILD_EXTRAS OFF CACHE BOOL "" FORCE)
  FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG ${ASTE_JUCE_COMMIT} # JUCE 8.0.13
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

  juce_add_plugin(LoopL01
    VERSION ${PROJECT_VERSION}
    COMPANY_NAME "Aste Internal"
    BUNDLE_ID "invalid.aste.loop-l01"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    COPY_PLUGIN_AFTER_BUILD FALSE
    PLUGIN_MANUFACTURER_CODE Aste
    PLUGIN_CODE Lop1
    FORMATS VST3
    VST3_CATEGORIES Fx Delay
    PRODUCT_NAME "Loop L-01"
  )
  target_sources(LoopL01 PRIVATE apps/loop/loop_plugin.cpp)
  target_include_directories(LoopL01 PRIVATE apps/loop)
  target_link_libraries(LoopL01 PRIVATE loop_dsp juce::juce_audio_utils)
  target_compile_definitions(LoopL01 PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_VST3_EMULATE_MIDI_CC_WITH_PARAMETERS=0
  )
  target_compile_options(LoopL01 PRIVATE -Wall -Wextra -Wpedantic -Werror)
  if(APPLE)
    add_custom_command(TARGET LoopL01_VST3 POST_BUILD
      COMMAND codesign --force --deep --sign - "$<TARGET_BUNDLE_DIR:LoopL01_VST3>"
      COMMENT "Applying final ad-hoc signature after Loop VST3 manifest generation"
      VERBATIM
    )
  endif()

  juce_add_plugin(ImpulseI01
    VERSION ${PROJECT_VERSION}
    COMPANY_NAME "Aste Internal"
    BUNDLE_ID "invalid.aste.impulse-i01"
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    COPY_PLUGIN_AFTER_BUILD FALSE
    PLUGIN_MANUFACTURER_CODE Aste
    PLUGIN_CODE Imp1
    FORMATS VST3
    VST3_CATEGORIES Instrument Synth
    PRODUCT_NAME "Impulse I-01"
  )
  target_sources(ImpulseI01 PRIVATE apps/impulse/impulse_plugin.cpp)
  target_include_directories(ImpulseI01 PRIVATE apps/impulse)
  target_link_libraries(ImpulseI01 PRIVATE impulse_dsp juce::juce_audio_utils)
  target_compile_definitions(ImpulseI01 PUBLIC JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0 JUCE_VST3_EMULATE_MIDI_CC_WITH_PARAMETERS=0)
  target_compile_options(ImpulseI01 PRIVATE -Wall -Wextra -Wpedantic -Werror)
  if(APPLE)
    add_custom_command(TARGET ImpulseI01_VST3 POST_BUILD
      COMMAND codesign --force --deep --sign - "$<TARGET_BUNDLE_DIR:ImpulseI01_VST3>"
      COMMENT "Applying final ad-hoc signature after Impulse VST3 manifest generation"
      VERBATIM)
  endif()

  juce_add_plugin(HarmonicH01
    VERSION ${PROJECT_VERSION}
    COMPANY_NAME "Aste Internal"
    BUNDLE_ID "invalid.aste.harmonic-h01"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    COPY_PLUGIN_AFTER_BUILD FALSE
    PLUGIN_MANUFACTURER_CODE Aste
    PLUGIN_CODE Har1
    FORMATS VST3
    VST3_CATEGORIES Fx EQ
    PRODUCT_NAME "Harmonic H-01"
  )
  target_sources(HarmonicH01 PRIVATE apps/harmonic/harmonic_plugin.cpp)
  target_include_directories(HarmonicH01 PRIVATE apps/harmonic)
  target_link_libraries(HarmonicH01 PRIVATE harmonic_dsp juce::juce_audio_utils)
  target_compile_definitions(HarmonicH01 PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
  )
  target_compile_options(HarmonicH01 PRIVATE -Wall -Wextra -Wpedantic -Werror)
  if(APPLE)
    add_custom_command(TARGET HarmonicH01_VST3 POST_BUILD
      COMMAND codesign --force --deep --sign - "$<TARGET_BUNDLE_DIR:HarmonicH01_VST3>"
      COMMENT "Applying final ad-hoc signature after Harmonic VST3 manifest generation"
      VERBATIM
    )
  endif()

  juce_add_plugin(SequenceS01
    VERSION ${PROJECT_VERSION}
    COMPANY_NAME "Aste Internal"
    BUNDLE_ID "invalid.aste.sequence-s01"
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    COPY_PLUGIN_AFTER_BUILD FALSE
    PLUGIN_MANUFACTURER_CODE Aste
    PLUGIN_CODE Seq1
    FORMATS VST3
    VST3_CATEGORIES Instrument Synth
    PRODUCT_NAME "Sequence S-01"
  )
  target_sources(SequenceS01 PRIVATE apps/sequence/sequence_plugin.cpp)
  target_include_directories(SequenceS01 PRIVATE apps/sequence)
  target_link_libraries(SequenceS01 PRIVATE sequence_dsp juce::juce_audio_utils)
  target_compile_definitions(SequenceS01 PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_VST3_EMULATE_MIDI_CC_WITH_PARAMETERS=0
  )
  target_compile_options(SequenceS01 PRIVATE -Wall -Wextra -Wpedantic -Werror)
  if(APPLE)
    add_custom_command(TARGET SequenceS01_VST3 POST_BUILD
      COMMAND codesign --force --deep --sign - "$<TARGET_BUNDLE_DIR:SequenceS01_VST3>"
      COMMENT "Applying final ad-hoc signature after Sequence VST3 manifest generation"
      VERBATIM
    )
  endif()

  juce_add_plugin(FieldF01
    VERSION ${PROJECT_VERSION}
    COMPANY_NAME "Aste Internal"
    BUNDLE_ID "invalid.aste.field-f01"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    COPY_PLUGIN_AFTER_BUILD FALSE
    PLUGIN_MANUFACTURER_CODE Aste
    PLUGIN_CODE Fie1
    FORMATS VST3
    VST3_CATEGORIES Fx Reverb
    PRODUCT_NAME "Field F-01"
  )
  target_sources(FieldF01 PRIVATE apps/field/field_plugin.cpp)
  target_include_directories(FieldF01 PRIVATE apps/field)
  target_link_libraries(FieldF01 PRIVATE field_dsp juce::juce_audio_utils)
  target_compile_definitions(FieldF01 PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_VST3_EMULATE_MIDI_CC_WITH_PARAMETERS=0
  )
  target_compile_options(FieldF01 PRIVATE -Wall -Wextra -Wpedantic -Werror)
  if(APPLE)
    add_custom_command(TARGET FieldF01_VST3 POST_BUILD
      COMMAND codesign --force --deep --sign - "$<TARGET_BUNDLE_DIR:FieldF01_VST3>"
      COMMENT "Applying final ad-hoc signature after Field VST3 manifest generation"
      VERBATIM
    )
  endif()

  juce_add_console_app(density_plugin_tests PRODUCT_NAME "Density plugin tests")
  if(APPLE)
    add_library(realtime_audit SHARED tests/realtime_audit_mac.cpp)
    target_compile_options(realtime_audit PRIVATE
      -Wall -Wextra -Wpedantic -Werror)
    target_link_libraries(density_plugin_tests PRIVATE realtime_audit)
  endif()
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

  juce_add_console_app(harmonic_plugin_tests PRODUCT_NAME "Harmonic plugin tests")
  if(APPLE)
    target_link_libraries(harmonic_plugin_tests PRIVATE realtime_audit)
  endif()
  target_sources(harmonic_plugin_tests PRIVATE
    apps/harmonic/harmonic_plugin.cpp
    tests/harmonic_plugin_tests.cpp
  )
  target_include_directories(harmonic_plugin_tests PRIVATE apps/harmonic)
  target_link_libraries(harmonic_plugin_tests PRIVATE
    harmonic_dsp juce::juce_audio_utils)
  target_compile_definitions(harmonic_plugin_tests PRIVATE
    ASTE_NO_PLUGIN_FACTORY=1
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
  )
  target_compile_options(harmonic_plugin_tests PRIVATE
    -Wall -Wextra -Wpedantic -Werror)
  add_test(NAME harmonic_plugin_tests COMMAND harmonic_plugin_tests)

  juce_add_console_app(sequence_plugin_tests PRODUCT_NAME "Sequence plugin tests")
  if(APPLE)
    target_link_libraries(sequence_plugin_tests PRIVATE realtime_audit)
  endif()
  target_sources(sequence_plugin_tests PRIVATE
    apps/sequence/sequence_plugin.cpp
    tests/sequence_plugin_tests.cpp
  )
  target_include_directories(sequence_plugin_tests PRIVATE apps/sequence)
  target_link_libraries(sequence_plugin_tests PRIVATE
    sequence_dsp juce::juce_audio_utils)
  target_compile_definitions(sequence_plugin_tests PRIVATE
    ASTE_NO_PLUGIN_FACTORY=1
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
  )
  target_compile_options(sequence_plugin_tests PRIVATE
    -Wall -Wextra -Wpedantic -Werror)
  add_test(NAME sequence_plugin_tests COMMAND sequence_plugin_tests)
  add_test(NAME sequence_editor_artifacts
    COMMAND sequence_plugin_tests --editor-artifacts
            "${CMAKE_CURRENT_BINARY_DIR}/sequence_editor_artifacts")

  juce_add_console_app(loop_plugin_tests PRODUCT_NAME "Loop plugin tests")
  if(APPLE)
    target_link_libraries(loop_plugin_tests PRIVATE realtime_audit)
  endif()
  target_sources(loop_plugin_tests PRIVATE
    apps/loop/loop_plugin.cpp
    tests/loop_plugin_tests.cpp
  )
  target_include_directories(loop_plugin_tests PRIVATE apps/loop)
  target_link_libraries(loop_plugin_tests PRIVATE loop_dsp juce::juce_audio_utils)
  target_compile_definitions(loop_plugin_tests PRIVATE
    ASTE_NO_PLUGIN_FACTORY=1
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
  )
  target_compile_options(loop_plugin_tests PRIVATE
    -Wall -Wextra -Wpedantic -Werror)
  add_test(NAME loop_plugin_tests COMMAND loop_plugin_tests)
  add_test(NAME loop_editor_artifacts
    COMMAND loop_plugin_tests --editor-artifacts
            "${CMAKE_CURRENT_BINARY_DIR}/loop_editor_artifacts")

  juce_add_console_app(impulse_plugin_tests PRODUCT_NAME "Impulse plugin tests")
  if(APPLE)
    target_link_libraries(impulse_plugin_tests PRIVATE realtime_audit)
  endif()
  target_sources(impulse_plugin_tests PRIVATE apps/impulse/impulse_plugin.cpp tests/impulse_plugin_tests.cpp)
  target_include_directories(impulse_plugin_tests PRIVATE apps/impulse)
  target_link_libraries(impulse_plugin_tests PRIVATE impulse_dsp juce::juce_audio_utils)
  target_compile_definitions(impulse_plugin_tests PRIVATE ASTE_NO_PLUGIN_FACTORY=1 JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
  target_compile_options(impulse_plugin_tests PRIVATE -Wall -Wextra -Wpedantic -Werror)
  add_test(NAME impulse_plugin_tests COMMAND impulse_plugin_tests)
  add_test(NAME impulse_editor_artifacts COMMAND impulse_plugin_tests --editor-artifacts "${CMAKE_CURRENT_BINARY_DIR}/impulse_editor_artifacts")

  juce_add_console_app(field_plugin_tests PRODUCT_NAME "Field plugin tests")
  if(APPLE)
    target_link_libraries(field_plugin_tests PRIVATE realtime_audit)
  endif()
  target_sources(field_plugin_tests PRIVATE
    apps/field/field_plugin.cpp
    tests/field_plugin_tests.cpp)
  target_include_directories(field_plugin_tests PRIVATE apps/field)
  target_link_libraries(field_plugin_tests PRIVATE
    field_dsp juce::juce_audio_utils)
  target_compile_definitions(field_plugin_tests PRIVATE
    ASTE_NO_PLUGIN_FACTORY=1
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0)
  target_compile_options(field_plugin_tests PRIVATE
    -Wall -Wextra -Wpedantic -Werror)
  add_test(NAME field_plugin_tests COMMAND field_plugin_tests)
  add_test(NAME field_editor_artifacts
    COMMAND field_plugin_tests --editor-artifacts
            "${CMAKE_CURRENT_BINARY_DIR}/field_editor_artifacts")

  juce_add_console_app(vst3_smoke_host PRODUCT_NAME "VST3 smoke host")
  target_sources(vst3_smoke_host PRIVATE apps/standalone-host/main.cpp)
  target_link_libraries(vst3_smoke_host PRIVATE
    juce::juce_audio_processors_headless)
  target_compile_definitions(vst3_smoke_host PRIVATE
    JUCE_PLUGINHOST_VST3=1
    JUCE_USE_CURL=0
    JUCE_WEB_BROWSER=0
  )
  target_compile_options(vst3_smoke_host PRIVATE
    -Wall -Wextra -Wpedantic -Werror)
  if(ASTE_HAS_NO_UNNECESSARY_VIRTUAL_SPECIFIER)
    target_compile_options(vst3_smoke_host PRIVATE
      -Wno-unnecessary-virtual-specifier)
  endif()
  add_dependencies(vst3_smoke_host DensityD01_VST3 HarmonicH01_VST3
                   SequenceS01_VST3 LoopL01_VST3 ImpulseI01_VST3
                   FieldF01_VST3)
  add_test(NAME density_vst3_host
    COMMAND vst3_smoke_host "$<TARGET_BUNDLE_DIR:DensityD01_VST3>"
            "Density D-01" "Drive" 11)
  add_test(NAME harmonic_vst3_host
    COMMAND vst3_smoke_host "$<TARGET_BUNDLE_DIR:HarmonicH01_VST3>"
            "Harmonic H-01" "Harmonic" 12)
  add_test(NAME sequence_vst3_host
    COMMAND vst3_smoke_host "$<TARGET_BUNDLE_DIR:SequenceS01_VST3>"
            "Sequence S-01" "Pressure" 85)
  add_test(NAME loop_vst3_host
    COMMAND vst3_smoke_host "$<TARGET_BUNDLE_DIR:LoopL01_VST3>"
            "Loop L-01" "Capture" 20)
  add_test(NAME impulse_vst3_host
    COMMAND vst3_smoke_host "$<TARGET_BUNDLE_DIR:ImpulseI01_VST3>"
            "Impulse I-01" "Energy" 368)
  add_test(NAME field_vst3_host
    COMMAND vst3_smoke_host "$<TARGET_BUNDLE_DIR:FieldF01_VST3>"
            "Field F-01" "Forever" 9)

  if(Python3_Interpreter_FOUND AND APPLE)
    set(ASTE_DENSITY_PACKAGE
        "${CMAKE_CURRENT_BINARY_DIR}/packages/Density-D01-${PROJECT_VERSION}-internal-macos-universal.zip")
    add_custom_target(density_package
      COMMAND Python3::Interpreter
              "${CMAKE_CURRENT_SOURCE_DIR}/tools/package_density.py" create
              --bundle "$<TARGET_BUNDLE_DIR:DensityD01_VST3>"
              --metadata "${CMAKE_CURRENT_BINARY_DIR}/build-metadata.json"
              --notices "${CMAKE_CURRENT_SOURCE_DIR}/docs/THIRD_PARTY_NOTICES.md"
              --security "${CMAKE_CURRENT_SOURCE_DIR}/docs/security/dependency-audit.json"
              --output "${ASTE_DENSITY_PACKAGE}"
      BYPRODUCTS "${ASTE_DENSITY_PACKAGE}"
      DEPENDS DensityD01_VST3
              "${CMAKE_CURRENT_SOURCE_DIR}/tools/package_density.py"
              "${CMAKE_CURRENT_SOURCE_DIR}/tools/check_dependency_security.py"
              "${CMAKE_CURRENT_SOURCE_DIR}/docs/security/dependency-audit.json"
              "${CMAKE_CURRENT_SOURCE_DIR}/docs/THIRD_PARTY_NOTICES.md"
      COMMENT "Creating deterministic internal Density development package"
      VERBATIM)
  endif()
endif()
