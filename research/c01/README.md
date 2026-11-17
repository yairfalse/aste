Playable update: C-01 is now included in the root universal build and installed as version 0.10.0. See the [product guide and installed verification](../../docs/products/contamination/README.md). The original isolated lab commands and evidence below remain historical.

# C-01 Contamination laboratory and playable candidate

Start with the [research report](../../docs/research/c01/RESEARCH.md),
[technical/state contract](../../docs/research/c01/CONTRACT.md),
[interface study](../../docs/research/c01/INTERFACE.md),
[measured evidence](../../docs/research/c01/EVIDENCE.md) and
[MVP/remaining plan](../../docs/research/c01/PLAN.md).

No external DSP dependencies. C++20, CMake ≥3.25. Run from repository root:

```sh
cmake -S research/c01 -B build-c01-lab -DCMAKE_BUILD_TYPE=Release
cmake --build build-c01-lab --parallel 3
ctest --test-dir build-c01-lab --output-on-failure
build-c01-lab/c01_lab build-c01-listening-final
build-c01-lab/c01_family build-c01-family-final
build-c01-lab/c01_benchmark > build-c01-benchmark.csv
python3 research/c01/provenance.py build-c01-listening-final
python3 research/c01/provenance.py build-c01-family-final
```

Pack directories must be new. Lab writes 38 studies × raw/matched = 76 WAVs,
events, state traces, measurements, alias study, obscured-ID answer key and blank
ratings. Family adds four matched references. Audio is 48 kHz stereo IEEE float32,
14 seconds per file. Do not read `key.csv` before rating. Benchmark alone with no
competing builds/renders; CPU fractions are wall time/audio duration, not DAW
deadline guarantees. Render timings inside the listening CSV are approximate;
use the dedicated benchmark for budgeting.

Reproduction check (same compiler/platform):

```sh
build-c01-lab/c01_lab build-c01-reproduction
python3 research/c01/provenance.py build-c01-listening-final build-c01-reproduction
cmake -S research/c01 -B build-c01-sanitized -DCMAKE_BUILD_TYPE=Debug -DC01_SANITIZERS=ON
cmake --build build-c01-sanitized --target c01_tests --parallel 3
ctest --test-dir build-c01-sanitized --output-on-failure
```

A standalone instrument build reuses the root build's pinned JUCE source; it does
not fetch another version, change the root build, install or overwrite a plugin:

```sh
cmake -S research/c01 -B build-c01-plugin -DCMAKE_BUILD_TYPE=Release \
  -DC01_PLUGIN=ON \
  -DC01_JUCE_SOURCE="$PWD/build-plugin-universal/_deps/juce-src"
cmake --build build-c01-plugin --target ContaminationC01_VST3 c01_plugin_tests --parallel 3
ctest --test-dir build-c01-plugin -R c01_plugin --output-on-failure
```

Bundle: `build-c01-plugin/ContaminationC01_artefacts/Release/VST3/Contamination C-01.vst3`.
Editor artifact: `build-c01-plugin/c01-editor.png`. Current development binary is
arm64; a universal build can set `-DCMAKE_OSX_ARCHITECTURES='arm64;x86_64'` but has
not been validated in this cycle. Installation and Cubase evaluation are pending.

This machine's CMake is at
`/private/tmp/aste-build-tools/lib/python3.14/site-packages/cmake/data/bin/cmake`
(`ctest` alongside it). Its command-line toolchain requires the explicit SDK:

```sh
export CPLUS_INCLUDE_PATH=/Library/Developer/CommandLineTools/SDKs/MacOSX15.5.sdk/usr/include/c++/v1
# Add these flags to each configure, retaining CPLUS_INCLUDE_PATH during JUCE builds:
# -DCMAKE_OSX_SYSROOT=/Library/Developer/CommandLineTools/SDKs/MacOSX15.5.sdk
# '-DCMAKE_CXX_FLAGS=-isystem /Library/Developer/CommandLineTools/SDKs/MacOSX15.5.sdk/usr/include/c++/v1'
```

These are local toolchain workarounds, not project requirements. No dependency
installation or network access is required when the pinned checkout is present.

Source hashes include the dirty-workspace family comparators and shared headers.
Generated WAVs and build directories are ignored by repository policy. Retained
text evidence lives in `docs/research/c01/evidence`; hashes let a later run detect
source drift. The engine remains in this research subtree to preserve one source
of truth during development; promotion to a product processor comes after the
listening decision, not a second divergent copy now.
