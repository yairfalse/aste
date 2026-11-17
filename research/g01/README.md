# G-01 — Grain: experimental granular network

The musician advanced this work into the [playable Grain G-01 effect](../../docs/products/grain/README.md), version 0.9.0. The original lab remains independent.

Research cycle 1, 2026-11-16. This is an independent C++20 laboratory and an
interaction study, **not a new installed VST3**. Existing Aste products and their
public parameter contracts are untouched. The musician’s
[artistic context](../../docs/MUSICAL_INTENT.md) guides the experiments; musical
acceptance is still open.

Start with the [research and architecture report](REPORT.md),
[interaction prototype](interface.html), [interaction specification](INTERACTION.md),
and [experiment evidence](EVIDENCE.md).

## Run

```sh
cmake -S research/g01 -B build-g01 -DCMAKE_BUILD_TYPE=Release
cmake --build build-g01 --parallel 6
ctest --test-dir build-g01 --output-on-failure
build-g01/g01_lab build-evidence/g01/fresh-listening
python3 research/g01/analyze.py build-evidence/g01/fresh-listening
node research/g01/ui_checks.js

# Offline external input: PCM16/24/32 or float32 mono/stereo WAV, <=30 seconds.
# 44.1–192 kHz; original rate retained; 20 seconds of silence appended.
build-g01/g01_lab --input recording.wav build-evidence/g01/external-fresh

# Run with no builds/sanitizers competing for CPU.
build-g01/g01_lab --benchmark build-evidence/g01/benchmark-fresh

cmake -S research/g01 -B build-g01-sanitized -DG01_SANITIZERS=ON
cmake --build build-g01-sanitized --parallel 4
ctest --test-dir build-g01-sanitized --output-on-failure
```

On this laptop the compiler requires:

```sh
export CPLUS_INCLUDE_PATH=/Library/Developer/CommandLineTools/SDKs/MacOSX15.5.sdk/usr/include/c++/v1
export SDKROOT=/Library/Developer/CommandLineTools/SDKs/MacOSX15.5.sdk
```

Every render command requires a fresh output folder. `core` has no JUCE, file
I/O, threading or product dependencies. Only the comparison executable links
actual Loop and Field DSP. `state.hpp` and `wav_input.hpp` are offline utilities.

The final local listening folder is `build-evidence/g01/final/`. It contains
34 stereo float WAVs, raw/output measurements, telemetry, analysis, hashes and
an experimental configuration. Six source files cover generated chord, noise,
percussion, impulse, tone and a **synthetic field-like** texture. The latter is
not a recording. No genuine field recording or musician listening assessment
was available in this cycle. The importer was exercised with the generated
stereo float fixture; other supported PCM encodings still need fixture coverage.

Open `interface.html` locally. Load the rendered `telemetry.csv` to replay actual
active-grain, stored-energy, output-energy and capture readouts. UI controls edit
a separate research interaction model; they do not modify or synthesize the
recorded sound. Its exported JSON is explicitly a UI-study format, separate from
the versioned DSP lab configuration. Browser/VoiceOver/layout validation is
still open; Node tests exercise the interaction model only.
