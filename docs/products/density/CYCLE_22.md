# Cycle 22 — 4x FIR length comparison

## What changed

Made the prototype FIR length prepare-time configurable without allocation and
compared 16, 32, 48, and 64 taps per phase. Expanded the alias measurement from
one tone to four tones spanning 7–15 kHz.

## Why

The 64-tap prototype missed the CPU budget. Shortening the same filter needed
measured transition-band behaviour before it could be considered a safe fix.

## Evidence

- All candidates measure zero variable-block sample delta and latency exactly
  equal to taps per phase.
- Maximum fundamental gain shift remains below 0.007 dB.
- Worst folded energy improves from -22.014 dBc at 16 taps to -50.920 dBc at
  64 taps.
- Three isolated five-run CPU medians span 1.054–1.061% at 16 taps,
  1.956–1.973% at 32, 2.801–2.827% at 48, and 3.692–3.705% at 64.
- Processing all four lengths performs zero observed heap allocations.
- Release and ASan/UBSan VST3 suites both pass 11/11; sanitizer timing remains
  intentionally disabled while all four filter lengths and tones are checked.

## Risks

No candidate meets both the provisional 1.0% CPU budget and credible
transition-band alias suppression. Measurements remain scalar, 48 kHz, and
single-tone; the prototype is still disconnected from production.

## Next step

Compare a two-stage sparse half-band 4x topology against this direct FIR before
considering custom SIMD, an ADR, or production integration.
