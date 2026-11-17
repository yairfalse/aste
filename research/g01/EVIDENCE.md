# Completed experiments and evidence — cycle 1

Local macOS arm64, AppleClang 17, C++20 Release. New code is under
`research/g01`; no plugin or deployment was added. Source snapshots/hashes and
logs accompany the local rendered evidence. No subjective listening claim is
made.

| Experiment | Completed artifact | What it establishes / limit |
|---|---|---|
| G1 | Three window WAVs; linear/Sinc16 alias probe; pool tests | Grain/window/read implementation works; one alias probe is not full anti-alias validation |
| G2 | Matched direct-B and cascade-A→B WAVs | B actually captures A’s output; 24 downstream capture publications; matched RMS sample difference 0.02237, not a musical verdict |
| G3 | Parallel and cross-route WAVs, partition tests | Directed routing and mixtures operate independently of call size |
| G4 | Full 1.5-level matrix with noise and 144-grain bound | Finite clipped/normalized output, counted overload; not a proof every configuration decays |
| G5 | Quarter-speed independent/varispeed WAV pair | Tone peak ~445.31 vs 111.33 Hz in chosen window; independent mode is colored OLA, not perfect pitch-preserving stretch |
| G6 | Centred/scattered percussion | Side/mid power changes from numerical zero to -5.54 dB; mono RMS 0.00762→0.00535, so width has a level consequence |
| G7 | Matched inherited/eroded cyclic WAVs | Low-pass/nonlinear damage and identity scatter are recaptured; no automatic perceptual similarity measure |
| G8 | Performed network, 180-second render and 180-row telemetry CSV | Nonzero late output with frozen seed A and continuing B/C capture; not indefinite unfrozen self-sustain |
| G9 | Core sample-offset changes, PPQ/stop/seek simulation, configuration restore | Core semantics and control state only; no JUCE boundary, real DAW or Cubase test |
| G10 | Six sources, actual Loop/Field and single-node comparisons, WAV importer | Generated inputs only; genuine field-recording audition and human comparison remain open |

## Measured audio

The final pack has 34 float stereo WAVs at 48 kHz, including six source files,
two alias probes and one three-minute example. Matched comparisons are G2,
G5, G7 and the two Grain-versus-Loop/Field pairs. They use gated LUFS matching
and common peak headroom; other files retain raw levels. Never infer preference
from an unmatched level change. Metrics and waveform hashes are retained.

The alias test uses a 17 kHz tone read at 2×. The 14 kHz folded-band power is
73.04 dB lower with the selected short sinc reader than linear interpolation.
This is a narrow, finite-window probe including the rest of the graph; it does
not cover nonlinear erosion, all pitches, windows, source-bank boundaries or
other rates. Independent-time tone peak error/sidebands are explicitly visible
in the 445.31 Hz result relative to the 440 Hz input.

In the three-minute run, stereo RMS is approximately 0.00875 at 30 s, 0.00788 at
90 s, and 0.00867 at 179 s. Input is present for two seconds; node A freezes at
one second and therefore holds the already published seed, while B/C continue
capturing. The seed is explicit retained memory, not autonomous noise or a
hidden synth. The long example is a reproducible performance scenario, not
proof it is interesting for three minutes.

## Correctness and real-time checks

- Release CTest: 3/3 tests pass (contracts, render reproducibility, UI model). Contract suite covers six rates
  (44.1/48/88.2/96/176.4/192 kHz), blocks 1/7/127/511/2048, sample-offset
  routing changes and node clear, repeated deterministic renders, both readers,
  non-finite input/controls, invalid command batches, freeze sealing partial
  memory, immutable published length, and clear.
- New/delete audit reports zero callback allocation/free. macOS interposition
  self-calibrates deliberate mutex/file/write calls, then reports none in the
  audited processing calls.
- ASan/UBSan build: both C++ suites and the UI model pass (3/3) with no diagnostics.
- Free frozen output matches exactly despite DAW seek/tempo changes. Synced
  grain births stop when host stops; memory is retained.
- Experimental text control state round-trips and rejects junk/unknown versions
  transactionally. Audio-memory recall is not implemented.
- External importer exercised with the generated stereo float WAV, with a
  separately rendered 20-second tail. PCM16/24/32 parsing is implemented but
  not yet fixture-covered; no actual field recording was supplied.
- `node ui_checks.js` passes graph/matrix, route connection, selection, freeze,
  seeded position mutation and clear-feedback model checks. This uses a small
  DOM test double; it is not a real browser, screenshot or accessibility audit.

## Performance evidence and proposed budget

Standalone benchmark after builds complete, short captures, density 240,
2-second grains, all three nodes saturated at a 48-voice pool each. Counts show
dropped births; benchmarking a bounded pool is not a promise of 480 overlapping
grains. Timings include the complete core process call; OS scheduling outliers
are retained. No realtime thread priority or Cubase multicore conditions.

At 48 kHz / 127 frames: median linear 187.75 µs and Sinc16 607.63 µs, versus
2645.83 µs deadline. At 192 kHz / 127: medians 189.29 / 607.79 µs, versus
661.46 µs deadline. Sinc16 p99 is 668.33 µs and max 736.96 µs there; at 2048
frames it also has deadline outliers. It is **rejected as an unrestricted
high-rate 144-voice product mode**. Linear has substantially more headroom,
but long-tail callback timing and multiple instances need host testing.

The recorded benchmark uses 0.1-second banks to isolate active-grain work;
full four-second prepared memories measure 13,870,280 bytes at 48 kHz. Expected
scaling is linear with sample rate; allocation is explicit in prepare. A
production quality budget should start with the measured linear candidate and
experimentally cap high-rate sinc voices; no universal CPU promise is made.

## Evidence location

`build-evidence/g01/final/`: WAVs, `measurements.csv`, `analysis.json`,
`telemetry.csv`, `SHA256.json`, `configuration.g01`.

`build-evidence/g01/benchmark-final/benchmark.csv`: standalone timing run
quoted above; `benchmark-current/benchmark.csv` repeats it after the final sync
phase handling adjustment. Both runs retain all outliers.
Other cycle folders retain earlier experiments instead of replacing their data.
The evidence manifest records current source hashes and logs. Source/lab build
reproduction is in [README](README.md).
