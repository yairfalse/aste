# Aste musical-power audit laboratory

This standalone C++20 lab calls all six unchanged production cores. It generates
two separately named source variants from **single-occurrence checked anchors**:
Sequence retriggers a note arriving during release; Loop wraps its existing
motion phase after the three frequencies' common 1000-second period. Harmonic's
alternative is an existing +12 dB Input/−12 dB Output gesture, not a new graph.
No released target, parameter ID, preset meaning or session schema is changed.

Read the [audit](../../reports/Aste%20musical%20power%20audit.md),
[evidence](../../docs/research/power/EVIDENCE.md),
[capability matrix](../../docs/research/power/CAPABILITY_MATRIX.md),
[backlog](../../docs/research/power/BACKLOG.md) and
[actual-editor review](../../docs/research/power/INTERFACE.md).
The [listening index](../../docs/research/power/LISTENING.md) lists the six
generated archives, checksums and completed export checks.

```sh
cmake -S research/power -B build-power-lab -DCMAKE_BUILD_TYPE=Release
cmake --build build-power-lab --parallel 3
build-power-lab/power_lab --check
build-power-lab/power_lab --measure build-power-evidence
build-power-lab/power_lab --pack build-power-listening
build-power-lab/power_lab --benchmark build-power-evidence/benchmark.csv
build-power-lab/power_partition_probe
ctest --test-dir build-power-lab --output-on-failure
```

**The full last command is currently expected to fail**, without `WILL_FAIL`
or an inverted assertion: `power_existing_host_automation` exposes the unchanged
Sequence host-gate callback-partition defect. Its separate `--host-check`
command exits 1. Do not interpret the passing candidate/measurement check as
a clean existing-product suite. The PPQ-bias branch in `power_partition_probe`
is a causal diagnostic only, not the proposed fix or the listening renderer.

For sanitizer checks use a separate build with `-DPOWER_SANITIZERS=ON`.
The standalone Sykli contract runs only `power_measurement_contract` in Release
and ASan/UBSan; its proof is deliberately scoped to the experimental checks.
Run the full CTest above separately and retain its known failure. The root
six-plugin suite and expiring dependency security policy remain independent.

The pack destination must be new; existing responses are never overwritten.
Each product has `audio/`, unmodified float `source/`, measurements, an answer
key and blank responses. All WAVs are stereo 48 kHz and eight seconds. Inputs
cover percussion, bass, oscillator, noise, chords, a generated layered passage
and sparse events. They are procedural diagnostics, not supplied recordings
or real project excerpts. Loop settings include an explicit first two-second
capture; an untouched empty instance is not claimed to generate a loop.
Impulse reproduces the current adapter's Steady Start defaults, not its
historical core-only defaults. MIDI notes deliberately fall at different
callback offsets; fixed seeds do not conceal that existing bug.

Settings are explicit recipes in `render.hpp`, not renamed factory presets or
listener-approved sweet spots. Fast automation uses a rational 25 ms grid,
rounded upward to the first available sample, including 44.1 kHz. Within a
block targets remain constant; source cores still expose their own host timing.
Source cutoffs and sparse-event edges are intentional. Maximum sample delta
alone is not a processor click detector.

Matching uses 48 kHz BS.1770-style K weighting, 400/100 ms windows, −70 LUFS
absolute and −10 LU relative gating. It iteratively remeasures after scaling,
then lowers ONE shared target if any comparison exceeds 0.8 sample peak.
Raw LUFS below the absolute gate stays unknown; nonzero audio can still be
scaled and measured for audition. Silence is not invented as a finite level.
This independently checked implementation is not externally certified and
does not measure true peak. Use matched `audio/` for listening; raw float
`source/` diagnostics can exceed unity.

Begin with Sequence's gapped versus overlapping MIDI comparisons, Loop's
Wow-clock pair, and Harmonic's character/linked-gain pair on bass and chords.
Then compare the conservative/extreme and dry references. Record character,
responsiveness, control and confidence separately from preference. Test useful
results in a real Cubase project before accepting a production change.

Benchmark only after other builds, renders and archiving finish. It records
callback time and nonzero output, excluding setup/analysis; Loop includes its
snapshot-enabled path. Runtime heap size for Loop is explicitly unmeasured,
not zero. These short local trials are not supported-hardware or DAW guarantees.
