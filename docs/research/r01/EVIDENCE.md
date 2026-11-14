# R-01 completed evidence

Recorded 2026-11-14 on Apple M4 Pro, arm64 macOS, AppleClang
21.0.0.21000334, C++20 Release unless stated. Baseline:
`8a603dcbc59b52a58637fe6305676efc5a406f80`. Work is isolated on
`research/r01-resonators`; existing product DSP, parameter contracts, session
formats and root CMake are unchanged. **No human listening assessment, native
R-01 editor, Cubase trial or musical-success claim has been completed.**

## Artifacts and reproduction

The [laboratory commands](../../../research/r01/README.md) regenerate every
study. [The report](../../../reports/Resonator%20harmonic%20memory.md) gives
alternatives and the musical go/no-go decision; the
[contract](LAB_CONTRACT.md) defines actual semantics and bounds.

The retained local listening pack is
`build-r01-listening-final/` in the research worktree
`/Users/yair/projects/aste/build-r01/worktree`; its ZIP is
`build-r01-listening-final.zip`. These generated binaries are intentionally not
committed. The pack contains **28 variants × raw/matched = 56 stereo float32
WAVs**, each 48 kHz and 8 seconds, plus a manifest, blank response sheet, answer
key, events, per-bank state observations, measurements and SHA256SUMS.
Apple `afinfo` independently recognized the WAV format and exact duration.
Two independent pack runs were byte-identical (`diff -rq`, exit 0), before the
checksum file was added. Source provenance in the pack is
`b0c739887dd2935bc19f7ec531acb662d7a7b635`; the subsequent benchmark-only change
does not alter pack audio. ZIP SHA-256:

```text
b9293436c036ab9a734329b9094b7f12948a1aab2d621c6350c541fc4c0f8a1c
```

Raw measurements are committed as [listening.csv](evidence/listening.csv).
No ordinary R-01 study activated the energy/write guard. R-01 raw peak range
was 0.00608–0.20962. Matched files target RMS 0.05 but a peak cap of 0.8 takes
precedence, so they are **not all exactly equal RMS or perceptual loudness**.
Keep the raw files when evaluating envelope shape and gain differences.

| Experiment | Actual rendered/measured comparison |
|---|---|
| R1 | Equal-energy white, approximate pink, shaped noise, impulse, short granular-noise packets and synthetic external chirp into the same modal structure |
| R2 | Modal/waveguide white and impulse use identical input arrays; waveguide dispersion 0, 0.5, 0.8; C3 tuning/decay and processing measurements |
| R3 | Harmonic, stretched, inharmonic; minor7, suspended, minor9 and ambiguous four-tone score; cents offsets and per-mode damping |
| R4 | Modal coupling 0/3/15 Hz; changing-coupling autonomous norm oracle; coupled retained-memory fixture |
| R5 | Reset, Shared, Partial, Overlap and coupled Shared all at nominal T60=8 s, strikes 1.5 s apart; fixed-linear superposition control |
| R6 | Sample-offset held-note replacement fixture, root/chord snapshots, seeded mutation; no live MIDI/DAW claim |
| R7 | 0.18 s stabs, 120 s nominal decay, persistent/release audio; 12 one-minute strong-drive/abrupt-control stress runs; nonlinear fold-band probe |
| R8 | Four unmodified Sequence voices into one Field; identical unpitched noise into Field |
| R9 | Two visually inspected static SVG wireframes and interaction specification; not executable or usability-tested |

The MIDI fixture and Sequence reference use integer notes rather than the chord
snapshot's cents offsets. Sequence uses its own excitation/envelopes, so this is
a practical family comparison, not an equivalent-control null test. Reference
product state/guard counters are recorded as `unknown`, not fabricated zeros.
`events.csv` records the tagged input score; unused fields retain the event
type's defaults and must not be read as applied controls. Reference event rows
are score anchors, not a complete product MIDI trace. Actual R-01 frequencies
and separate old/new-bank energy appear in `energy.csv`.

## Correctness and robustness

Release and AddressSanitizer/UndefinedBehaviorSanitizer CTest each passed
**3/3**: contracts, complete stereo render reproducibility, and long stress.
The standalone Sykli contract passed both tasks; it is a research-core gate,
not a substitute for the repository's security review. Core partition tests compare blocks 1/7/127/511/2048 at
44.1/48/88.2/96/176.4/192 kHz. All 26 candidate studies additionally match
sample-for-sample at 127/511-frame stereo blocks. Fixed modal response matches
an independent analytic sum within 1e-8; T60=1 gives energy 1e-6 after one
second within 1e-12. Fixed shared linear memory matches independent-response
superposition within 1e-8: a **negative differentiation result**, not a novel
nonlinear effect.

Allocation/free and macOS lock/open/write audits calibrate their own probes,
then observe zero audited callback operations. Ten independently compiled
faults were all detected: wrong modal decay, nonorthogonal coupling, broken
reset, missing projection, missing finite-input guard, MIDI release regression,
growing waveguide feedback, stale delay history, invalid control acceptance
and a callback allocation. See [mutations.csv](evidence/mutations.csv). This
is targeted mutation coverage, not proof that every possible bug is detected.

The tests caught and fixed two real issues: same-sample note replacement
incorrectly released the new chord, and rolling waveguide energy accounting
left approximately 4.67e-10 after decay. The former has a failing-before unit
regression; the latter has a failing-before long-stress result and compensated
summation fix. Neither was resolved by weakening the threshold or muting audio.
Static Clang analysis reported no core diagnostics; clang-format and whitespace
checks passed. These checks do not replace independent review or platform tests.

[stress.csv](evidence/stress.csv) records 60 seconds per architecture/rate:
50 seconds of unit-amplitude drive, endpoint damping/dispersion/decay changes
and root changes every 128 samples, silence thereafter, Release at 55 seconds.
All 12 runs stayed finite, rejected/sanitized no valid input and ended below
1e-12 state energy. Maximum observed peaks: modal 0.399865, waveguide 0.404898.
Safety guards were active in these intentionally extreme cases; bounded output
is **not** a claim of transparent or desirable behavior. Modal final norm was
zero after denormal cleanup; waveguide residuals were at most 1.12e-22 on this
build. The waveguide meter excludes filter state; neither meter is a physical
energy oracle shared by both architectures.

The unmodified-product baseline core suite passed 50/51. The complete VST3
build and suite executed 68 tests: all 66 non-documentation/non-security tests
passed. Its documentation check initially failed only because this evidence
file was still being assembled; the completed-documentation rerun passed 1/1.
Thus the combined results leave 67/68 passing, not an all-green full suite.
The existing security review expired **2026-11-07**, so `dependency_security`
remains a known baseline failure. Its date/policy was not changed to obtain a
green result. No R-01 plugin was built or installed.

## Tuning, decay and aliasing

[tuning.csv](evidence/tuning.csv) measures a single C3 fundamental
(130.81278265 Hz) with impulse drive at all six rates. Both architectures were
within **0.200 cents in this FFT estimator**. The shared estimator bias is
visible at high rates; this is not a guaranteed tuning error across the entire
keyboard, dispersion range or automation trajectory. A bin-centred sine
independently calibrates FFT magnitude and peak location. Waveguide damping
includes interpolation/filter losses; equal nominal T60 does not mean equal
partial decay. The two energy columns must not be compared across architectures
as if their units were identical.

[alias.csv](evidence/alias.csv) reports a restricted 4.5 kHz drive experiment.
Power in five predicted folded odd-harmonic bands (±2 FFT bins) is divided by
total window power. Four-times-rate output is low-pass filtered before
decimation; excitation amplitude is divided by four at the higher rate.

| Engine / drive | Base-rate fold bands | Filtered 4× fold bands | Base guard interventions |
|---|---:|---:|---:|
| Modal, weak, decaying | −153.55 dB | −154.73 dB | 0 |
| Modal, strong, persistent | −101.81 dB | −156.62 dB | 93,076 |
| Waveguide, weak, decaying | −156.46 dB | −149.97 dB | 0 |
| Waveguide, strong, persistent | −43.82 dB | −70.55 dB | 41,952 |

The waveguide clamp is an audible-risk nonlinear stage, not automatically a
usable saturation design. These selected bands are not total alias power;
rate-dependent interpolation/damping and changed nonlinear operating points
prevent treating the difference as a pure alias subtraction. Weak-run figures
approach numerical leakage. Noise high-band energy is not labeled aliasing.
Retuning/coupling sideband aliasing remains uncharacterized. No production
oversampling path or “alias-free” claim is justified by this diagnostic.

## Processing and storage

[benchmark.csv](evidence/benchmark.csv) records five 5-second trials per row
below at source `bcc68aa`. Median is elapsed wall time / audio duration, as a
percentage of one real-time thread—not OS process CPU or a host meter. Both
banks remain sounding; chords replace every 16 blocks, damping changes every
block, persistence is enabled, modal coupling is 20 Hz. Every trial's output
energy probe is nonzero. Compilation, ZIP creation and other lab work were
finished before the recorded timing run.

| Engine | Rate / block | Median real-time fraction | Largest observed callback |
|---|---|---:|---:|
| Modal | 48 kHz / 128 | 0.2985% | 41.625 µs |
| Modal | 48 kHz / 511 | 0.2649% | 42.583 µs |
| Modal | 96 kHz / 128 | 0.4815% | 26.250 µs |
| Modal | 96 kHz / 511 | 0.4689% | 51.167 µs |
| Waveguide | 48 kHz / 128 | 0.1548% | 22.583 µs |
| Waveguide | 48 kHz / 511 | 0.1440% | 28.833 µs |
| Waveguide | 96 kHz / 128 | 0.3108% | 19.750 µs |
| Waveguide | 96 kHz / 511 | 0.2928% | 29.125 µs |

These local short offline trials are not worst-case scheduling/deadline proof.
The original every-block-reset timing fixture was rejected because it could
clear a waveguide before feedback returned. No result from it is used above.
Neither candidate invokes its safety guard during the recorded benchmark;
strong-drive paths are exercised separately by stress tests.

Each Engine object is 3,088 bytes on this build. A fresh modal Engine has zero
heap sample storage. Waveguide buffers require 96,256 bytes at 48 kHz and
384,256 bytes at 192 kHz, allocated in prepare; this excludes the allocator's
overhead and offline renderer vectors. There is no added lookahead/block
buffer, but waveguide propagation is part of its sound. No host latency API
exists. Practical next-research ceilings are 1% of one thread at 48 kHz/128 and
512 KiB core state at 192 kHz; these measurements fit, but are not release
guarantees or evidence of cross-platform performance.

## Decision and incomplete evaluation

Continue **modal-led musical research only**. Preserve waveguide and
Sequence→Field as controls; do not choose a permanent hybrid, stable public
parameter set or finished plugin identity. The report defines a composition
and repeatability task to test whether retuning/damping/transferring stored
energy gives a useful benefit over existing products. Blank response sheets
mean no listener evidence exists yet. Native accessibility, interaction timing,
host automation, portable sessions, larger pitch/alias sweeps and live
playability are not completed results.
