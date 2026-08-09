# Cycle 5 — Harmonic H-01 internal beta

## What changed

Candidate 3 became an independent four-band production processor and testable
VST3. Harmonic now owns its 12 parameters, schema-1 state, six factory starting
points, scalable industrial editor, meters, adapter tests, and regression
fixtures. The shared ABI smoke host now accepts either product, and the macOS
handoff builds and installs universal Density and Harmonic bundles together.

## Why

The bounded state-variable candidate passed its frozen laboratory gate and was
the smallest measured topology worth testing in music. A product-owned serial
graph exposes actual band interaction without coupling Harmonic to Density or
inventing a family-wide engine.

## Evidence

- Core tests cover all six sample rates, required block sizes, finite fuzzed
  input/parameters, exact neutral, clean cuts, deterministic reset, variable
  blocks, stereo identity, zero latency, monotonic canonical H3, and no callback
  allocation.
- The product report is finite with zero latency across 12 six-rate fixtures.
  Broad fixtures add 2.362–2.371 dB RMS and sculpt fixtures add 1.409–1.467 dB.
- The 48 kHz / 128-sample worst-case automation benchmark measures 0.682702%
  median of one M4 Pro core across five runs (0.671615–0.758637%).
- Adapter tests cover stable parameter text, deterministic state, malformed and
  fuzzed state, presets, mono/stereo, bypass, meters, finite audio, callback
  allocation/I/O/lock auditing, editor scaling, and essential control exposure.
- The VST3 ABI host loads Harmonic, sees 12 parameters and zero latency,
  round-trips state, and processes irregular blocks.
- The universal arm64+x86_64 bundle passes strict code-signature verification,
  Steinberg VST3 SDK 3.8.0 extensive validation at 537/537, and pluginval 1.0.4
  strictness 10 with seed `0xa501` across the required rate/block matrix.
- Both user installers copy the universal bundles into a temporary VST3 folder
  and re-verify their signatures and two architecture slices after installation.

## Risks

Candidate 3 is not listening-selected in a DAW. Four serial nonlinear boosts
may become too hard, bright, or interactive on real mixes. Current protection
is a finite safety clamp, not a limiter. There is no oversampling, true-peak
claim, Developer ID signature, notarization, native Intel performance result,
or Cubase/Ableton compatibility result yet.

## Next step

Complete
[`MUSIC_MACHINE_TEST.md`](MUSIC_MACHINE_TEST.md) in Cubase and Ableton with
level-matched material before changing the provisional DSP.
