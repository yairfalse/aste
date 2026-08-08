# Cycle 32 — Blind oversampling audition pack

## What changed

Added one DSP-lab command that renders four anonymous, deterministic 1x-versus-
73/33 stereo pairs with a separate answer key, response sheet, and measurement
manifest.

## Why

The retained oversampling candidate should earn further optimization through a
level-matched listening preference, not alias measurements alone.

## Evidence

- Both paths are aligned to the measured 44-sample prototype latency.
- Pair RMS mismatch is at most 0.000000171 dB.
- Every pair has a common -1 dBFS peak ceiling.
- Null RMS ranges from -109.058 to -47.440 dBFS across the four fixtures.
- Fixed seed `0xD0132` reproduces the anonymous assignments.
- A repeated render produced byte-identical hashes for all 11 pack files.
- All 18 core and all 20 full VST3 Release tests pass.
- All 20 full Address/UndefinedBehavior sanitizer tests pass.

## Risks

The fixtures are synthetic and no listening preference has been recorded.
Whole-file RMS matching can conceal short-term loudness differences, so the
response notes remain important.

## Next step

Audition each pair blind on headphones, monitors, and in mono; complete the
response sheet before opening the answer key. Do not change the production DSP
until those preferences are logged.
