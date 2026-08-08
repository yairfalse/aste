# Cycle 19 — Nonlinear alias baseline

## What changed

Exposed the existing saturation and controlled-clipping sample functions to the
DSP lab without changing their transfer curves. Added a dependency-free alias
report for all six supported sample rates and a CTest measurement check. Direct
invalid sample and drive inputs now resolve to finite outputs at the shared
function boundary.

## Why

The unoversampled nonlinear path was a documented hypothesis-level risk. An
oversampling strategy needs measured alias behaviour from the actual production
functions rather than a generic waveshaper assumption.

## Evidence

- At 44.1 and 48 kHz, folded 3rd-through-63rd harmonic energy measures
  -19.303 dBc for saturation, -18.790 dBc after the crush clip, and -33.511 dBc
  for the protection clip.
- At 192 kHz the same measurements improve to -57.347, -54.592, and
  -61.761 dBc respectively.
- Existing production golden and consistency tests remain unchanged.
- Release and ASan/UBSan VST3 suites both pass 9/9. Their alias reports are
  byte-identical with SHA-256
  `5bc43333977f1e32c69d842f019c35da2f630edef83b20846fa344debb074bf3`.

## Risks

This is one strong coherent tone near 7 kHz and a bounded harmonic sum, not
total broadband alias power or a perceptual threshold. No oversampling factor,
anti-alias filter, latency policy, or quality-mode mapping has been selected.

## Next step

Compare 2x and 4x oversampled offline references, including filter latency and
base-rate residual alias energy, before writing an oversampling ADR.
