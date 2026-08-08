# Cycle 45 — Blind stereo-link auditions

## What changed

Added six deterministic anonymous A/B/C trials comparing independent, partial,
and fully linked detection through the production signal path. The pack keeps
the answer key separate and provides a response sheet for stability, preference,
confidence, mono, and stereo observations.

## Why

Cycle 44's objective metrics cannot determine whether measured image movement
is musically acceptable. The smallest useful next test is controlled listening
to the same fixtures without revealing each link mode.

## Evidence

- The pack contains 18 four-second, 48 kHz stereo WAV files covering centered
  kick, hard-panned percussion, correlated program, decorrelated ambience,
  mono-in-stereo, and polarity-inverted material.
- Each trial is RMS-matched within 0.000001 dB and normalized to a common
  -1 dBFS sample peak within 0.000001 dB.
- Centered, mono-in-stereo, and polarity-inverted null controls are byte-identical
  across all three modes and null below -300 dBFS.
- The asymmetric trials remain measurably distinct: independent-to-partial
  nulls range from -18.387 to -42.119 dBFS and partial-to-linked nulls from
  -24.093 to -41.897 dBFS.
- Repeated generation with seed `852293` is byte-identical. Release and sanitizer
  answer keys share SHA-256 `de456b7fbac8f76860b5a8ee13ba5f563e10b7be3099d732267b69e20bbcb4b1`;
  measurements share `ea97fda2d045897d5888254de036418513962b91c98c12ddd923e68dcacc9063`.
- Release and Address/UndefinedBehavior sanitizer matrices pass 28/28.
- Production golden and six-rate variable-block evidence remain unchanged.

## Risks

The trials use synthetic material at one aggressive operating point. Exact
level matching deliberately removes loudness differences, and the pack does
not replace evaluation on real musical projects. Listening responses are still
pending, so release gate 18 remains open.

## Next step

Complete `build-plugin/density-stereo-stability-auditions/responses.csv` blind
on monitors, headphones, and in mono, then record the perceptual result.
