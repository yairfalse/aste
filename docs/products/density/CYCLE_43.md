# Cycle 43 — Automation audition pack

## What changed

Added a deterministic anonymous A/B renderer for Drive, Attack, Blend, and
simultaneous endpoint automation. It writes eight stereo WAVs plus separate
listening instructions, response sheet, answer key, and measurements.

## Why

The deterministic curvature gate now passes, but the release gate explicitly
requires listening for avoidable zipper noise under aggressive automation.

## Evidence

- Four 4-second pairs exercise full-range changes every 127 samples at 48 kHz.
- Production is compared with retained prior trajectories: one-stage 5 ms
  Drive, block-constant Attack, and one-stage 5 ms Blend.
- Each pair is RMS matched within 0.000001 dB and shares -1 dBFS peak
  normalization within 0.000001 dB.
- Production curvature improves over its reference by 30.964 dB for Drive,
  26.851 dB for Attack, 25.118 dB for Blend, and 16.974 dB simultaneously.
- Seed `852291` reproduces the answer assignment, measurements, and all eight
  WAV files byte-for-byte across repeated, Release, and sanitizer renders.
- The measurement CSV SHA-256 is
  `33f94da986add4891db5843d09c1147066423217bbac491fa7792367ab89ebe1`.
- Release and Address/UndefinedBehavior sanitizer matrices pass 26/26,
  including plugin lifecycle and calibrated callback safety audits.
- Production golden and six-rate variable-block evidence remain unchanged.

## Risks

This is an intentionally extreme synthetic stress test, not musical material.
It cannot replace slower automation or DAW-host listening, and no listening
result is recorded until a human completes the response sheet blind.

## Next step

Listen to the four pairs before opening the answer key, complete the response
sheet, and record whether either file has clicks, buzz, or rough edges.
