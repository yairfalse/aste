# Cycle 07 — Programme-dependent release

## What changed

Corrected the shared comparison fixture so every impulse and burst begins from
reset detector state. Added a lab-only programme-memory detector whose recent
activity continuously moves release time between 60 and 600 ms.

## Why

The previous 200 ms event gaps were shorter than detector recovery and
contaminated supposedly isolated measurements. Programme-dependent release must
also respond to event duration, not merely instantaneous envelope level.

## Evidence

- Pre-impulse and pre-burst gain reduction are exactly 0 dB; detector state is
  explicitly reset at both boundaries.
- Programme-memory sustained, impulse, and burst depth match current within
  0.043 dB.
- It releases 292.000 ms sooner after a 10 ms burst and 111.979 ms later after
  sustained material.
- Four four-way audition sets are RMS-matched within 0.001 dB.
- All sixteen WAVs parse as 48 kHz IEEE Float and reproduce identical SHA-256
  hashes after regeneration.
- All earlier detector tables were regenerated and corrected.

## Risks

The 100/600 ms memory constants and 60–600 ms release range are research values.
The fixture remains synthetic, mono, and limited to one operating point.

## Next step

Run the four-way blind audition, then prototype the required hybrid
feed-forward topology without adding a product-facing selector.
