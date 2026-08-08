# Cycle 26 — Supported-rate Kaiser sweep

## What changed

Extended the Kaiser comparison to 44.1, 48, 88.2, 96, 176.4, and 192 kHz.
Kaiser β3 and β5 each receive 702 deterministic measurements across 1–20 kHz
and three input levels.

The folded-harmonic helper now records a finite -300 dBc floor when none of the
measured odd harmonics crosses Nyquist, preventing invalid infinity and NaN
rows at high sample rates.

## Why

The 48 kHz result was insufficient for selecting coefficients used by every
supported host rate.

## Evidence

- β3 has the better alias 95th percentile at five of six sample rates.
- Across the full matrix, β3 reaches -43.800 dBc p95 versus -43.338 dBc for β5.
- β3 has 62/702 points above -50 dBc versus 71/702 for β5.
- β5 limits maximum fundamental shift to 0.017055 dB. β3 reaches +0.106176 dB
  near 19.5 kHz at 44.1 kHz and therefore fails the provisional 0.1 dB bound.
- The full report SHA-256 is
  `5555081d4f04cf7749e509ec0744b636e69ed866319475bf4b191b44eb1882eb`.
- Release and ASan/UBSan VST3 suites both pass 15/15; sanitizers use the
  documented 108-point-per-window smoke matrix.

## Risks

The folded-harmonic metric is not broadband error or a perceptual score. β3's
alias advantage conflicts with β5's flatter passband, and the 113/33 topology
still exceeds the 1% CPU budget. Neither candidate is in production.

## Next step

Measure the linear passband magnitude and phase of β3 and β5, concentrating on
18–20 kHz at 44.1 kHz, before selecting coefficients.
