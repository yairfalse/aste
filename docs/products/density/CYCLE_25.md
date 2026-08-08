# Cycle 25 — Dense Kaiser sweep

## What changed

Added a deterministic 351-point sweep comparing Blackman, Kaiser β3, and
Kaiser β5 at the fixed 113/33 half-band topology. The sweep covers 39 target
frequencies and three input levels.

## Why

The four-tone comparison could miss sidelobe peaks. A denser matrix is needed
before choosing coefficients for a production oversampling candidate.

## Evidence

- Kaiser β3 has the best worst case at -17.578 dBc and the best 95th percentile
  at -43.215 dBc.
- β3 and β5 each have 20/117 points above -50 dBc, versus 24/117 for Blackman.
- All worst cases occur near 8.001 kHz at 0.90 peak, where the third harmonic
  enters the base-rate Nyquist transition; the windows differ there by at most
  0.022 dB.
- Maximum fundamental shift is 0.066467 dB for β3, 0.011067 dB for β5, and
  0.003364 dB for Blackman.
- The full Release CSV SHA-256 is
  `9be9ae7132a5e1dfcc9ab4ac52f725314fc87e141f98cb9b9ed42d33894be574`.
- Release and ASan/UBSan VST3 suites both pass 14/14; the sanitizer sweep uses
  the documented six-frequency smoke matrix.

## Risks

Only 48 kHz is covered. The alias metric sums selected folded odd harmonics,
not total broadband error, and the near-Nyquist transition dominates the
minimax result. β3 still exceeds the current 1% CPU budget at this topology.
No candidate is integrated into production.

## Next step

Measure β3 and β5 across all six supported sample rates before selecting a
coefficient set or changing production latency.
