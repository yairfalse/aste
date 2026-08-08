# Cycle 28 — β5 length crossover

## What changed

Added a combined report for β5 first-stage lengths 65, 73, 81, 89, 97, and
113 taps with the second stage fixed at 33 taps. It measures alias rejection,
44.1 kHz passband and phase, latency, and stereo CPU.

## Why

The selected 113/33 coefficient set was too expensive. The useful boundary is
the longest filter that preserves quality while remaining below the 1% local
CPU target.

## Evidence

- 65/33 meets CPU at 0.923–0.929% but is rejected by 0.124032 dB passband
  deviation.
- 73/33 measures 0.958–0.961% CPU, -36.313 dBc alias p95, 0.039351 dB maximum
  deviation, and 44 samples latency.
- 81/33 measures 0.986–0.998% CPU, -38.102 dBc alias p95, 0.038969 dB maximum
  deviation, and 48 samples latency.
- 89/33 and longer exceed 1% in every run.
- Every topology remains linear-phase within 0.000001 degrees after latency
  compensation.
- The deterministic no-timing report SHA-256 is
  `931f2ff7ca10931519a8e377933e6b94aa8e6333a44f2722a1de9d39b40b47a2`.
- Release and ASan/UBSan VST3 suites both pass 17/17; sanitizer length tests use
  the reduced matrices and skip only timing.

## Risks

CPU was measured only on the M4 Pro and 81/33 has negligible margin. Alias was
measured at 48 kHz and passband at 44.1 kHz; neither 73/33 nor 81/33 has passed
the complete supported-rate quality matrix. Production remains unchanged.

## Next step

Run 73/33 and 81/33 through the six-rate alias and linear-response matrices,
then retain at most one production candidate.
