# N-01 cycle 1 evidence

Research evidence, not a product acceptance or subjective listening verdict.

## Baseline and scope

Baseline commit: `8a603dcbc59b52a58637fe6305676efc5a406f80`.
The unmodified core built in Release with AppleClang 21.0.0 on arm64 and passed
50/51 CTest gates. `dependency_security` failed because its existing review
expired on 2026-11-07. It has not been renewed, bypassed or weakened here.

The experiment builds independently from `research/n01`. No existing product
source, public parameter, plugin identity, state schema or JUCE boundary changes.
The offline comparison executable alone compiles existing Impulse and Loop DSP;
`n01_core` does not link another product. The repository's macOS real-time audit
is reused unchanged by the test executable.

## Completed correctness checks

Release and ASan/UBSan pass the contract and stereo-render reproducibility tests.
Analytic hits at 9,000 / 15,000 / 18,000 samples check negative microtiming and
accelerating repeats. Scheduler output and traces match at six rates from
44.1 to 192 kHz and irregular callback sizes. Fifteen listening scenarios match
bit-for-bit in stereo and event traces at 127 and 511 frames. Nested Suspend also
matches a one-sample reference. Tempo segments, stop/restart and explicit seeks
match a one-sample reference across 7/127/128/511/2048-frame partitions.

Further checks cover finite values, invalid setup, missing/nonfinite host time,
zero frames, empty/oversized/overflowing capture requests, wrapped selections,
frozen-bank ownership, reverse/shorten/finite duration, Continue return, Suspend
offset, Rejoin, silence precedence, command ordering and trace overflow reporting.

Seven isolated faulty builds are rejected: early scheduler, disabled suspension,
wrong capture bank, disabled VOID, ignored timing validity, always-accepted
probability and callback allocation. The probability mutation initially survived;
a fractional-probability acceptance/rejection test closed that real coverage gap.
The mutation runner never edits the working source. Runtime guards self-calibrate
with a deliberate allocation/free and lock/open/write before auditing processing.
The audited processing run observes zero C++ new/delete and zero intercepted OS
calls. This covers the instrumented paths, not every possible libc/system API.

## Listening material and interpretation

The generator produces 17 variants, 34 WAVs (raw plus obscured IDs), event and
gesture traces, edit-boundary measurements and blank responses. Each is sixteen
seconds, stereo float32, 48 kHz. Apple's `afinfo` independently reads the WAV format
and duration correctly. No supplied recordings, copyrighted samples or imported
audio are used.

Comparisons include constant versus accelerating repetition; local/master/nested
capture; Continue versus Suspend; reverse and shorten; hard versus soft edits;
isolated Pressure/Impact/Dust; identical event traces driving Impulse Low/Body/Burst;
and Impulse into Loop. The last pair is a capability comparison, not equivalent
controls or an identical phrase scheduler. Existing instruments are not modified.

Raw metrics measure sample peak, stereo RMS, maximum adjacent-sample difference
and exact two-channel silence fraction. Silence fraction is not a perceptual
silence detector. Maximum delta is not a click audibility measure: Dust can dominate
it. `edits.csv` instead isolates action boundaries and the following 64 samples
on the left channel. Neither establishes subjective smoothness.

Blind copies aim at stereo RMS 0.06, capped at peak 0.85; raw files remain
unchanged. The isolated Dust fixture hits the peak cap and is not fully RMS-matched.
RMS is not perceived loudness, especially for sparse material. No human responses
have been collected; musical distinctiveness and preference remain unknown.

## Performance scope

Capture audio is 672,000 bytes at 48 kHz, 1,344,000 at 96 kHz and 2,688,000
at 192 kHz, plus fixed engine state. Memory is reserved in setup; capture swaps
bank ownership without copying. Recording history refills after every capture.
This exceeds the general Aste 256-KiB target, explicitly as a lab trade-off only.
There is no lookahead or output delay; the sample interval containing an event
owns its onset (less than one sample early), not a host-reported plugin latency.

The benchmark runs five eight-second trials for each rate/block/workload pair:
48/96 kHz, 127/128/511 frames, sparse study versus three dense eight-event tracks
with eight repeats and four fresh captures per block. Clock reads are outside
processing. Wall-time/audio-duration is an approximate one-core occupancy, includes
driver/timer overhead, and is not an OS CPU counter. Maximum block time is a noisy
observed maximum, not a worst-case bound. The checksum prevents dead-code removal.
### Recorded local results

DSP/render source commit `625131eb6e177159d7d301efd45b0b1a18700578`, Apple M4 Pro,
arm64, AppleClang 21.0.0.21000334, CMake Release, 2026-11-14. Existing plugin builds
and tests had finished before timing. This is still a shared desktop, not an
isolated real-time rig; no warm-up is discarded from the five trials.
The subsequent `587fc0b` change fixes only the response CSV's empty-column count.
Its regenerated WAVs and numerical results are unchanged.

| Workload | Rate / block | Median one-core wall-time occupancy | Five-run range | Largest observed block |
|---|---|---:|---:|---:|
| Sparse study | 48 kHz / 128 | 0.165749% | 0.165641–0.166141% | 10.333 µs |
| Dense nested | 48 kHz / 128 | 0.319416% | 0.316190–0.320380% | 44.334 µs |
| Sparse study | 96 kHz / 128 | 0.294863% | 0.291803–0.295413% | 30.833 µs |
| Dense nested | 96 kHz / 128 | 0.563342% | 0.550674–0.630402% | 80.917 µs |

[All 60 trials](evidence/benchmark.csv) include 127/511-frame results, source
commit, compiler, allocation size and checksum. Fixed engine state is 3,264 bytes
in this build, excluding vector storage and allocator overhead. Local CPU
results are below the proposed 1% target; memory is above the general budget.
No supported-hardware or loaded-DAW gate has therefore been satisfied.

[Raw audio measurements](evidence/measurements.csv) show exact stereo silence
around 18.75% for ordinary phrase fixtures and 24.16% for Suspend/Rejoin fixtures.
Impulse comparators lack N-01's final VOID gate; their absence of exact digital
silence is not evidence of worse music. Raw levels also differ substantially,
hence the separate matched copies.

[Action-boundary measurements](evidence/edits.csv) distinguish deliberate cuts
from smoothed entry: at sample 40,800, nested hard capture jumps about 0.0971 on
the left channel; nested soft capture has zero first-sample discontinuity. This
does not prove the rest of the transition is inaudible. Direct Rejoin can still
produce a measured jump (about 0.0221 in the first Suspend fixture).

[Mutation results](evidence/mutations.csv) record seven successful compilations
and seven expected test exits of 1. Unfaulted contracts pass. The frozen-bank,
timing and suspension tests are not merely checks that some output exists.

### Repository gates and reproducibility

- Baseline core: 50/51 checks passed; existing security review expired.
- Existing arm64 VST3 build: succeeded with pinned JUCE, AU disabled. Its full
  68-check run passed 66 checks; then-incomplete report links failed documentation
  checking alongside the expired security review. After finishing the report,
  `documentation_links` passed on rerun: 67/68 checks now have passing evidence.
  The security failure remains unresolved. No product source changed.
- Independent N-01 Release and ASan/UBSan contracts and 15-study stereo
  reproducibility checks pass; Clang static analysis reports no findings.
- Two independently generated complete packs compared byte-for-byte equal.
  After the CSV correction, audio, traces and numerical output remain equal;
  only the manifest commit and blank-response formatting differ.
- Both SVGs are XML-valid and were rendered/visually inspected. No native
  interaction, assistive-technology or Cubase test is implied.

The local Sykli contract explicitly declares its source inputs and runs the two
research configurations. Use `sykli verify --commit` against its emitted receipt;
it does not replace or erase the failing repository security gate. Nothing has
been merged, tagged, installed or assigned a product version.

## Limits

No Cubase run, native input/accessibility test, Intel result, supported-oldest-Mac
measurement, alias analysis, arbitrary tempo-ramp reconstruction, imported audio,
session serializer or live gesture recorder is claimed. Hidden intra-buffer host
jumps are unknowable from a block-only context. Seek/Rejoin invalidates captured
history. Smooth mode is a fixed-sample taper and transition experiment; very short
fragments lose amplitude, and direct event resets/VOID/Rejoin can intentionally cut.
Static wireframes are illustrative compositions, not DSP screenshots.
