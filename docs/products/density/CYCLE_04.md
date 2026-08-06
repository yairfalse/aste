# Cycle 04 — Detector comparison

## What changed

Added a deterministic DSP-lab comparison between the production peak detector
and a lab-only RMS-like detector with 15% fast peak influence. The command
exports both gain-reduction traces as CSV and prints summary measurements.

## Why

Density must not commit to its first detector. The alternative is calibrated
to the same sustained sine reduction so transient and recovery differences can
be compared without a simple level mismatch.

## Evidence

- Sustained gain reduction differs by 0.030 dB.
- The candidate applies 3.055 dB less reduction to an isolated full-scale
  impulse and 1.698 dB less to a 10 ms burst.
- The candidate returns below 1 dB of reduction 54.667 ms sooner after the
  burst.
- The generated CSV contains 192,000 deterministic sample traces plus its
  header; the comparison is now a CTest test.
- Release and ASan/UBSan suites pass all three core, comparison, and plugin
  tests.

## Risks

The fixture covers a sine, impulse, and short burst at one operating point.
It does not establish musical superiority, response across frequency/level,
or stereo behavior. The candidate is intentionally absent from product DSP.

Cycle 7 corrected these figures by resetting detector state between events;
the earlier 200 ms gaps allowed recovery carry-over.

## Next step

Render level-matched audition files for transient, bass-heavy, dense, and
ambient fixtures, then decide whether the candidate deserves a temporary
product experiment.
