# Cycle 27 — Linear passband and phase

## What changed

Added a linear processing path to the existing half-band prototype and a
deterministic magnitude/phase report for Kaiser β3 and β5. The full report
contains 462 points per window across every supported sample rate.

## Why

Cycle 26 could not distinguish filter passband ripple from nonlinear alias
effects. Density needs both controlled rejection and mastering-grade gain
consistency.

## Evidence

- β3 reaches 0.127896 dB maximum magnitude deviation; β5 reaches 0.020496 dB.
- At 44.1 kHz, β3 peaks at +0.127896 dB near 19.5 kHz. β5 remains between
  -0.020496 and +0.018805 dB.
- Both windows have at most 0.000001 degrees latency-compensated phase error and
  retain the measured 64-sample delay.
- β5 gives up 0.462 dB in aggregate alias p95 but is more than six times flatter
  by maximum magnitude deviation. β5 is selected as the coefficient candidate.
- The report SHA-256 is
  `a61825b8ae4339746d0dc812315ef555d56a225327b8a4501ff059bac537a6e2`.
- Release and ASan/UBSan VST3 suites both pass 16/16; Cycle 25/26 report hashes
  remain unchanged.

## Risks

This is a coherent steady-state test, not a listening result. β5's 113/33
topology still measures about 1.3% of one core against the 1% default budget.
Production remains 1x with zero latency.

## Next step

Measure shorter β5 first-stage filters to find the smallest topology that can
meet the CPU budget without materially degrading alias rejection or passband
flatness.
