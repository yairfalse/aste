# Non-Machine N-01 — temporal instrument laboratory

Research only. No plugin target, product identifier, stable parameter contract,
or change to an existing instrument is authorized by this experiment.

The question is whether three small sound objects, independent local cycles,
captured fragments and explicit phrase-time ownership enable controllable
rhythmic composition that Impulse and Loop cannot already provide conveniently.
Silence is programmed structure, not a missing event to fill with randomness.

Original research baseline: `8a603dcbc59b52a58637fe6305676efc5a406f80`. The
experiment was developed on `research/n01-temporal-lab` without modifying
existing products. It is now integrated alongside the subsequent musical-control
revision. Historical measurements still refer to their recorded source commit;
new comparison renders link the checked-out Impulse and Loop implementations.

## Results and boundaries

- [Research report and explicit time model](../../../reports/Non%20Machine%20time%20study.md)
- [Actual checks, measurements and limits](EVIDENCE.md)
- [Interaction specification](INTERACTION.md), [default view](default.svg),
  [held-fragment view](held.svg)
- [Standalone C++20 experiment](../../../research/n01/core.hpp)

N1–N6 have executable offline experiments; N7 has inspected static wireframes;
N8 has actual Impulse and Impulse→Loop comparison renders. No musical preference,
Cubase integration, native accessibility or stable session format is claimed.
The report defines a comparative composition/listening gate before product work.

## Reproduce from the repository root

```sh
cmake -S research/n01 -B build-n01-lab -DCMAKE_BUILD_TYPE=Release
cmake --build build-n01-lab --parallel 3
ctest --test-dir build-n01-lab --output-on-failure
build-n01-lab/n01_lab --pack build-n01-lab/listening
build-n01-lab/n01_lab --benchmark > build-n01-lab/benchmark.csv
build-n01-lab/n01_mutations research/n01 build-n01-lab/mutation-check /usr/bin/c++
```

Pack and mutation destinations must be new directories: existing listening
responses/evidence are never overwritten. The pack's `audio/` contains obscured
IDs; fill `responses.csv` before opening `answer-key.csv` or `source/`. Raw and
RMS-matched files are both retained. Listen quietly first. No preference is valid.
`events.csv` and `gestures.csv` expose the exact experimental score; action values
are lab enum ordinals, not a promised interchange format. Presets are explicit
fixtures in `lab.cpp`; the executable is not a general sequencer CLI.

The self-calibrating OS-call audit is macOS-specific. Sanitizers are available
with `-DN01_SANITIZERS=ON`. A local Sykli contract runs Release and sanitizer
checks without adding or changing hosted CI:

```sh
sykli run research/n01/sykli.json
sykli verify --contract research/n01/sykli.json --commit HEAD PATH_TO_RECEIPT
```

The declared inputs include sibling files required by Sykli's input rules;
passing this research contract is **not** proof of all six existing plugins.
Those retain the repository's own CMake/CTest, packaging and host gates.

## Explore without committing a product design

`studyPattern()` owns the sparse score; `Event` owns lock and repeat behavior.
`Fragment` owns immutable capture/read state; `Engine::process` owns host/phrase
time and replacement precedence. The lab-only renderer owns filesystem access,
WAV/CSV output, comparison routing and benchmark clocks. None of those services
is reachable from the processing callback.

Three decisions remain deliberately open to musical evaluation: whether Suspend
should resume offset or offer a quantized rejoin; how short fragments should be
windowed without erasing their attacks; and whether the small source set is
sufficiently different from Impulse. Current choices are experiments, not new
family-wide contracts.

## Cycle 2: articulation comparison

The [combined follow-up](../../../reports/N01-R01%20research%20cycle%202.md)
reproduces the original baseline and adds Restart, Carry and bounded 2 ms
Crossfade on the same accelerating impact train. The expanded pack contains
20 variants / 40 raw and matched WAVs; action-edge measurements, sanitizer
results, mutation tests and exact source fingerprints are retained in the
[cycle-2 evidence](../cycle2/README.md). Use the isolated runner there for a
new, complete listening pack rather than overwriting historical responses.
