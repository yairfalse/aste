# Cycle 10 — Blind detector listening pack

## What changed

Added a deterministic twenty-trial listening pack. Every research detector is
paired against the current detector across transient, bass, dense, and ambient
fixtures. The command emits anonymous A/B WAVs, a response sheet, source
renders, and a separate answer key.

## Why

The candidate set is closed. A repeatable blind comparison is now more useful
than adding another topology or choosing from measurements alone.

## Evidence

- Twenty trials cover all five candidates across all four fixture classes.
- Forty anonymous files are copied from twenty level-matched source pairs.
- Candidate order and A/B assignment use a stored deterministic seed.
- All forty blind WAVs parse as mono 48 kHz IEEE Float.
- Two regenerations produced the same aggregate SHA-256:
  `bcf761c5f1195bc11f6b5bdde0bd934f9c9474cbd0f0cb5ddb72b93036669149`.
- Release and ASan/UBSan builds each pass all four CTest checks.

## Risks

The fixtures are synthetic, mono, and self-administered. Opening the source
directory or answer key before responding defeats blinding. The deterministic
shuffle is reproducible, not cryptographically concealed.

## Next step

Complete the response sheet without inspecting the answer key. Use the result
to eliminate failures and move at most two finalists into production DSP
experiments.
