# Resonator R-01 comparative laboratory

Research only: no plugin, stable parameter identity or existing-product change.
Start with the [research report](../../reports/Resonator%20harmonic%20memory.md),
[evidence](../../docs/research/r01/EVIDENCE.md),
[provisional contract](../../docs/research/r01/LAB_CONTRACT.md) and
[interface study](../../docs/research/r01/INTERACTION.md).

```sh
cmake -S research/r01 -B build-r01-lab -DCMAKE_BUILD_TYPE=Release
cmake --build build-r01-lab --parallel 3
ctest --test-dir build-r01-lab --output-on-failure
build-r01-lab/r01_lab --pack build-r01-listening
build-r01-lab/r01_lab --tuning build-r01-lab/tuning.csv
build-r01-lab/r01_lab --alias build-r01-lab/alias.csv
build-r01-lab/r01_lab --stress build-r01-lab/stress.csv
build-r01-lab/r01_lab --benchmark build-r01-lab/benchmark.csv
build-r01-lab/r01_mutations research/r01 build-r01-mutations c++
```

Pack and mutation directories must not already exist. Build outputs/WAVs are
ignored; regenerate or use the retained local pack described in the evidence.
Benchmark alone with no competing build; wall-time fractions are not a DAW
deadline guarantee. Run the sanitizer suite and standalone declared checks with:

```sh
sykli run research/r01/sykli.json
```

The contract covers these research CMake/CTest targets, not every product or the
repository security gate. Its explicit inputs include comparison DSP, shared
headers and audit/test siblings. It does not modify the root build or CI.

`audio/` contains 28 obscured-ID RMS-matched examples; `source/` contains their
raw counterparts. All 56 WAVs are 8-second, 48 kHz stereo float32. The pack also
contains input-event traces, actual per-bank energy observations, measurements,
an answer key and blank responses. Do not open the key until rating. Assess
physical character, chord legibility, control of memory, identity and preference
separately; record confidence and "no difference" honestly. Numerical checks
and generated audio are complete; human listening is not claimed.

No dependency or plugin integration is needed. The lab links unmodified Sequence
and Field processors only for the four-voice chord reference and same-noise
reference. The R-01 core itself does not depend on either product.
