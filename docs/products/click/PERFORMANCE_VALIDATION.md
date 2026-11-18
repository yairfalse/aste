# Click performance refinement — 2026-11-18

This is the existing K-01, not a separate application. Product identity and
version remain unchanged. The implementation adds immediate event rolls and
master divisions, a persistent captured-audio strip in all three views, actual
PCM resampling, eight generations, sample-import and edit undo, SKEW, optional
Impact dynamics and project persistence. See the [workflow](README.md) and
[parameter/time/memory contract](CONTRACT.md).

## Completed evidence

- Release build: all 85 repository CTests passed, including the actual VST3
  smoke host. After subsequent Click-only fixes, its four tests passed again.
- ASan/UBSan: Click engine, memory and adapter suites passed. Later memory/adapter
  changes are covered by another targeted sanitizer run.
- ThreadSanitizer: simultaneous capture/printing and immutable PCM snapshots
  passed. This checks the memory engine, not every possible host/UI interleaving.
- Static analyzer: no diagnostics for the Click engine or memory engine.
- Callback checks: no allocation/free during capture or printing; existing
  plugin interposition also checks locks, file opens and writes in processing.
- UI: actual JUCE editor mouse events create an accelerating roll, select a
  captured loop and press/release 1/64. The real native views render at their
  1100×840 minimum. Keyboard/focus titles and exact value entry remain available;
  this is not a screen-reader usability study.
  Retained screenshots: [Sequence/captured audio](validation/performance-sequence.png)
  and [Sound](validation/performance-sound.png).
- Project workflow: capture, octave-down/filter/PCM processing, two actual
  resampling generations, selection undo/redo, destruction/reinstantiation and
  bit-exact PCM recall. The reopened capture is checked not to leak a live hit.
- State rejects oversized/malformed generation audio transactionally. Legacy
  state without extension fields restores neutral playback, zero SKEW, no
  durable capture and Original dynamics. Original Master/Bypass host positions
  remain 2590/2591; the total is now 2697 parameters.
- Exact 1/64 periods at 120 and 240 BPM, SKEW block-partition invariance, abrupt
  extremes, full-history refusal, oversize-print refusal, and sample-rate recall
  are checked. Existing host seek/stop and Continue/Hold checks still pass.

Tests caught and corrected: undo before the selection acknowledgement reached
audio; loss of negative-zero PCM bits during same-rate restore; a speed grid
that could not represent neutral 1×; and a live transient leaking into recalled
captured playback. Deliberately replacing printed PCM with zero, removing SKEW
and disabling Impact each failed the corresponding signal check. These negative
controls were compiled outside production sources in `build-recall/click-mutants`.

The installed arm64 bundle also passed the real VST3 smoke host: Click K-01,
2697 parameters, 8 samples latency, finite output and successful state exercise.
The historical [validation report](VALIDATION.md) describes an older build;
its universal/Rosetta/pluginval results are **not** new results for this change.

**Cubase has not been launched or validated in this environment.** No human
listening verdict has been supplied for these renders. Musical improvement,
Cubase automation gestures, full album-project reopening and extended performance
remain the creator's acceptance checks on the other Apple-silicon Mac.

## Reproducible audio

`click_plugin_tests` renders 48 kHz stereo 24-bit WAVs through the actual plugin
processor, using original procedural sources. Its output directory contains:

| Files | Purpose |
| --- | --- |
| `01_straight`, `02_skew` | Same source pattern with deterministic rhythmic deformation |
| `03_accelerating_roll` | Roll acceleration, pitch and velocity change |
| `05_master_64`, `06_release` | Rendered-audio repetition and return |
| `07_transpose_filter` | Octave-down capture through resonant morph/16-bit playback |
| `08_resample_generation_2`, `11_resample_generation_3` | Actual printed successors, not saved effect lists |
| `12_reopened_project` | Selected generation after destroying/recreating the processor |
| `13_original_dynamics`, `14_impact_dynamics` | Identical microscopic pattern and macro settings |
| `15_original_dynamics_matched`, `16_impact_dynamics_matched` | Same compressor comparison with RMS matched downward |

The other numbered WAVs are very short capture/undo/redo test probes, not music
examples. Nothing is claimed to reproduce an E-mu unit or the reference artists.

On this Mac, the compressor comparison had unadjusted RMS 0.00702346 (Original)
and 0.00538956 (Impact). The matched pair both have RMS 0.00538956, with peaks
0.103806 and 0.104808 respectively. This proves different processing at comparable
level, not that Impact sounds better. RMS matching is not perceptual LUFS matching.

The representative 48 kHz/256-frame workflow renders took roughly 2–7% of their
audio duration on this machine. This timing includes the adapter loop and audio
copying; it is not a worst-case deadline or Cubase CPU measurement. The separate
maximum-voice test reported mean fractions ~0.245/0.263/0.290 at 48/96/192 kHz,
with the existing 48/24/12-voice budgets. CPU varies with machine load. Raw values
are emitted in `performance_metrics.csv` and the engine test output.

Durable capture adds exactly 27,648,000 bytes of prepared PCM storage at 48 kHz
and 110,592,000 at 192 kHz. The existing engine remains additional: 125,239,296
bytes in the reported maximum-bank test. Saving/restoring allocates temporary
snapshots, encoded XML and replacement banks off audio, so peak session-load
memory is higher than DSP heap alone. No additional latency is introduced.

## Compatibility render

`tests/click_legacy_render.cpp` uses only the pre-existing Engine API. Compiled
with AppleClang against commit `a6291fc` and the new implementation, its six
factory-pattern fixtures (609,600 stereo frames per rate) produced identical
sample-bit fingerprints:

```text
48000 a154eba7fa47ae2b
192000 667a4cb95c36e85d
```

This is evidence for those fixtures, not an exhaustive equivalence proof.

## Reproduce locally

Use the repository's pinned JUCE source or normal CMake dependency setup:

```sh
cmake -S . -B build-recall -DCMAKE_BUILD_TYPE=Release -DASTE_BUILD_VST3=ON -DASTE_BUILD_AU=OFF
cmake --build build-recall --parallel 4
ctest --test-dir build-recall --output-on-failure
build-recall/click_plugin_tests build-recall/click_editor_artifacts
cmake -S . -B build-recall-sanitize -DCMAKE_BUILD_TYPE=Debug -DASTE_BUILD_VST3=ON -DASTE_BUILD_AU=OFF -DASTE_SANITIZERS=ON
cmake --build build-recall-sanitize --target click_engine_tests click_memory_tests click_plugin_tests --parallel 3
ctest --test-dir build-recall-sanitize -R 'click_(engine|memory|plugin)_tests' --output-on-failure
c++ -std=c++20 -O1 -g -fsanitize=thread -pthread -Iapps/click tests/click_memory_tests.cpp apps/click/click_memory.cpp -o build-recall/click_memory_tsan
build-recall/click_memory_tsan
```

The local delivery folder is `build-recall/click-delivery`: arm64 VST3, selected
WAV examples, measured CSV and this guide. No Intel build is claimed for this
iteration. The bundle is ad-hoc signed, not notarized. On the other Mac, copy it
to `~/Library/Audio/Plug-Ins/VST3`, avoid a duplicate older Click in the system
plugin folder, then rescan in Cubase.

## Bounds and remaining musical judgment

History is intentionally eight non-evicting generations, not unlimited undo or
disk streaming. A print freezes controls at invocation; automation afterwards
does not alter an in-flight generation. Seeks cancel incomplete prints. The
live compressor and older six gestures are upstream of durable captured playback;
they affect future captures, not already printed audio. Pitch/speed are varispeed,
and the intentionally degraded PCM choices can alias. Extreme filter/PCM quality
is not proven by finite-output tests. Old binaries cannot reproduce these new
features; backward compatibility means old projects opened in the new binary.

The next evaluation is musical: whether one or two gestures produce a useful
mistake, whether it remains controllable, and whether Impact earns its place
against Original at matched level. Those answers cannot come from these tests.
