# Compile source-identical, separately named controls before applying narrowly
# anchored experimental changes. Generated sources never enter a product target.
function(replace_once variable before after)
  set(value "${${variable}}")
  string(LENGTH "${value}" old_length)
  string(REPLACE "${before}" "" removed "${value}")
  string(LENGTH "${removed}" new_length)
  string(LENGTH "${before}" anchor_length)
  math(EXPR removed_length "${old_length} - ${new_length}")
  if(NOT removed_length EQUAL anchor_length)
    message(FATAL_ERROR "Candidate anchor is missing or ambiguous: ${before}")
  endif()
  string(REPLACE "${before}" "${after}" value "${value}")
  set(${variable} "${value}" PARENT_SCOPE)
endfunction()
foreach(product sequence loop)
  file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/candidate/${product}")
  foreach(extension hpp cpp)
    file(READ "${CMAKE_CURRENT_SOURCE_DIR}/../../apps/${product}/${product}_processor.${extension}" contents)
    string(REPLACE "namespace aste::${product}" "namespace aste::${product}_candidate" contents "${contents}")
    if(extension STREQUAL "cpp" AND product STREQUAL "sequence")
      replace_once(contents "envelope_.active(), glideMs);"
        "envelope_.active() && envelope_.stage_ != Envelope::Stage::release, glideMs);")
    elseif(extension STREQUAL "cpp" AND product STREQUAL "loop")
      # 1000 seconds closes all three existing decimal-frequency oscillators:
      # .47, 6.71 and .173 Hz. Wrapping at one second resets their phase early.
      replace_once(contents "modulationPhase_ -= std::floor(modulationPhase_);"
        "modulationPhase_ -= 1000.0 * std::floor(modulationPhase_ / 1000.0);")
    endif()
    file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/candidate/${product}/${product}_processor.${extension}" "${contents}")
  endforeach()
endforeach()
# Loop's header dependency remains source-owned and unmodified.
file(READ "${CMAKE_CURRENT_SOURCE_DIR}/../../apps/loop/tape_snapshots.hpp" contents)
string(REPLACE "namespace aste::loop" "namespace aste::loop_candidate" contents "${contents}")
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/candidate/loop/tape_snapshots.hpp" "${contents}")
add_library(power_candidates
  "${CMAKE_CURRENT_BINARY_DIR}/candidate/sequence/sequence_processor.cpp"
  "${CMAKE_CURRENT_BINARY_DIR}/candidate/loop/loop_processor.cpp")
target_include_directories(power_candidates PUBLIC "${CMAKE_CURRENT_BINARY_DIR}")
