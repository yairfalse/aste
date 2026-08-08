# Cycle 20 — Offline oversampling comparison

## What changed

Added a dependency-free offline 2x/4x interpolation and decimation reference to
the DSP lab. It compares residual alias energy, gain shift, filter length, and
equivalent causal latency against the existing 1x nonlinear baseline.

## Why

Cycle 19 proved that the base-rate crush path aliases strongly. Comparing
measured references identifies whether 2x is sufficient before production
real-time architecture is introduced.

## Evidence

- At 48 kHz, 2x reduces crush-path folded energy from -18.790 to -43.057 dBc,
  an improvement of 24.267 dB.
- 4x reduces it to -78.155 dBc, an improvement of 59.365 dB and 35.098 dB more
  suppression than 2x.
- Fundamental shift is below 0.00002 dB for both factors.
- The 64-tap-per-phase reference implies 64 base-rate samples of causal
  interpolation-plus-decimation latency at either factor.
- Release and ASan/UBSan VST3 suites both pass 10/10. Their oversampling reports
  are byte-identical with SHA-256
  `5275d9a12f9f8ca74e4aca813ecf865f7d97c5ce5c46a93e5a1b7e36104b2427`.

## Risks

The reference uses circular convolution and one coherent 7 kHz tone. It does
not measure real-time CPU, startup behaviour, automation, filter-state reset,
or dynamic quality transitions. Its filter is evidence, not production code.

## Next step

Prototype a Density-local 4x polyphase FIR path and benchmark its CPU, measured
latency, block invariance, and residual aliasing before writing the oversampling
ADR.
