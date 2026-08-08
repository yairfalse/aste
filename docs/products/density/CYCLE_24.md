# Cycle 24 — Kaiser coefficient comparison

## What changed

Added bounded prepare-time Kaiser coefficient generation to the sparse
half-band prototype and compared β3, β5, β7, β9, and β11 against Blackman at the
fixed 113/33, 64-sample topology.

## Why

The topology reduced arithmetic, but Blackman's transition shape limited the
quality result. Window tuning can change rejection without changing real-time
CPU, state size, or latency.

## Evidence

- Blackman worst folded energy is -43.643 dBc.
- Kaiser β3 improves the four-tone worst case to -51.524 dBc; β5 reaches
  -51.277 dBc.
- β5 measures -70.406 dBc at 8.5 kHz versus -65.258 dBc for β3, while β3 is
  0.247 dB better at the current worst tone.
- Maximum fundamental shift is 0.013571 dB for β3 and 0.006334 dB for β5.
- Runtime CPU remains the previously measured 1.293–1.303% because only
  prepare-time coefficients change. Latency remains 64 samples.
- Release and ASan/UBSan VST3 suites both pass 13/13. Their Kaiser reports are
  byte-identical with SHA-256
  `6b070ef69d06569b6850f5bafb2ca1f1c05de596d69aeb9f6c6d8fad51173317`.

## Risks

Four tones can miss Kaiser sidelobe peaks and make a minimax choice unstable.
The test still uses one input level and 48 kHz. Neither coefficient set is
integrated into production.

## Next step

Run a dense frequency and multi-level sweep for Blackman, Kaiser β3, and β5
before selecting coefficients or defining quality modes.
