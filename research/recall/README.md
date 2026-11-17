# Tape transport and source-memory recall checks

The implementation decision and limitations are in
[ADR 0020](../../docs/adr/0020-tape-transport-and-portable-memory.md).
This is correctness/compatibility evidence, not a musical listening verdict.

## Regression checks

Build with the repository's CMake configuration and JUCE source pin, then run:

```sh
cmake --build build-recall --target loop_tests loop_plugin_tests grain_engine_tests grain_plugin_tests click_plugin_tests -j 4
ctest --test-dir build-recall -R '^(loop_tests|loop_plugin_tests|grain_engine_tests|grain_plugin_tests|click_plugin_tests)$' --output-on-failure
```

Repeat in a Debug build configured with `-DASTE_SANITIZERS=ON` for ASan/UBSan.
The Grain core test also supports a direct ThreadSanitizer build:

```sh
c++ -std=c++20 -O1 -g -fsanitize=thread -pthread -Iapps/grain tests/grain_engine_tests.cpp apps/grain/grain_processor.cpp -o build-recall/grain_tsan
build-recall/grain_tsan
```

The tests cover retained Stop/Play/Release tape, transport recall and legacy
defaults, captured Grain sound before/after host preparation and rate changes,
repeated host preparation, malformed audio rejection, concurrent save/clear,
and Click sessions/scenes/sounds after moving the source file. Callback audits
include allocation/free counters and the existing macOS OS-call interposition.

## Differential render against the starting production code

The standalone fixture uses only the original public DSP APIs. It renders
exactly 50,800 frames per engine/rate, at 48 and 192 kHz, and prints
sample-bit fingerprints. It includes capture, playback and generational prints
in Loop, and recursive granular routing in Grain. Run both versions with the
same compiler; hashes are supplemental evidence, not portable golden values.

```sh
mkdir -p build-recall/reference
git show a5ce188:apps/grain/grain_processor.hpp > build-recall/reference/grain_processor.hpp
git show a5ce188:apps/grain/grain_processor.cpp > build-recall/reference/grain_processor.cpp
git show a5ce188:apps/loop/loop_processor.hpp > build-recall/reference/loop_processor.hpp
git show a5ce188:apps/loop/loop_processor.cpp > build-recall/reference/loop_processor.cpp
c++ -std=c++20 -O2 -Ibuild-recall/reference -Iapps/loop research/recall/compare_original.cpp build-recall/reference/grain_processor.cpp build-recall/reference/loop_processor.cpp -o build-recall/compare_before
c++ -std=c++20 -O2 -Iapps/grain -Iapps/loop research/recall/compare_original.cpp apps/grain/grain_processor.cpp apps/loop/loop_processor.cpp -o build-recall/compare_after
build-recall/compare_before > build-recall/reference-audio.txt
build-recall/compare_after > build-recall/current-audio.txt
diff -u build-recall/reference-audio.txt build-recall/current-audio.txt
```

Both versions produced the following on Apple silicon / AppleClang 21:

```text
48000 grain=10b0363b9a731d25 loop=e46bbe9c447978cb
192000 grain=a15392feaff4bc9d loop=8117182e32bca91b
```

This checks those fixtures only. It does not prove all control combinations,
Cubase session behavior, subjective transition quality or musical improvement.
