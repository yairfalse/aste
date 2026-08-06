# Cycle 08 — Hybrid feed-forward detector

## What changed

Added a lab-only hybrid feed-forward topology. Independent calibrated RMS-body
and fast-peak gain computers run in parallel; 35% of peak reduction above the
body reduction is admitted into the result.

## Why

Blending detector levels and blending computed gain reduction are not
equivalent. This topology tests whether a stable body path can preserve density
while limiting how strongly isolated peaks pull down the crush path.

## Evidence

- Sustained reduction matches current within 0.029 dB.
- Impulse reduction is 1.069 dB, between RMS/peak's 0 dB and current's 3.055 dB.
- Burst reduction is 13.960 dB, between RMS/peak's 13.527 dB and current's
  15.225 dB.
- Recovery falls below 1 dB after 88.021 ms for the burst and 59.500 ms after
  sustained material.
- Four five-way audition sets are RMS-matched within 0.001 dB.
- All twenty WAVs reproduce identical SHA-256 hashes after regeneration.

## Risks

The 35% peak contribution is a research calibration, not a product mapping.
Fast recovery may sound lively or unnaturally detached. Fixtures remain
synthetic, mono, and limited to one operating point.

## Next step

Prototype the final required feedback-inspired behavior in the lab, then rank
all six topologies for listening rather than expanding the candidate set.
