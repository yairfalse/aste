# Cycle 23 — Sparse half-band 4x prototype

## What changed

Added a fixed-state two-stage half-band 4x crush-path prototype. It exploits
half-band zero coefficients and compares six first/second-stage FIR lengths
under the same four-tone, block, latency, allocation, memory, and CPU protocol.

## Why

Cycle 22 showed that shortening the direct FIR destroys transition-band
suppression. A cascaded half-band topology can reduce arithmetic without custom
SIMD or weakening the measured filters.

## Evidence

- Equivalent alias performance uses 28–62% less CPU than the direct FIR.
- 65/33 taps meets the isolated oversampler CPU budget at 0.901–0.910%, with
  40 samples latency, but worst folded energy is only -28.492 dBc.
- 113/33 taps stays within the 64-sample latency target and reaches
  -43.643 dBc at 1.293–1.303% CPU.
- 129/33 matches the direct 64-tap-per-phase result at -50.919 dBc while using
  1.399–1.402% CPU, but latency rises to 72 samples.
- Extending the second stage from 33 to 65 taps changes worst alias by less than
  0.001 dB while adding about 0.38% CPU and 8 samples.
- Every configuration is allocation-free, finite, and sample-identical across
  fixed and variable block schedules; gain shift remains below 0.007 dB.
- Release and ASan/UBSan VST3 suites both pass 12/12. Sanitizers exercise every
  configuration and tone while skipping only the timing loop.

## Risks

No configuration yet meets both the 1.0% CPU target and stronger transition-
band suppression. Measurements remain 48 kHz and synthetic; the half-band
prototype is not integrated into the product processor.

## Next step

Compare prepare-time Kaiser-windowed half-band coefficients at the 113/33
64-sample configuration before considering SIMD, quality modes, or an ADR.
