# Cycle 35 — Output smoothing comparison

## What changed

Added a lab-only comparison of the current 5 ms Output smoother, 10 and 20 ms
one-pole variants, and a two-stage 3+3 ms exponential response. Production DSP
is unchanged.

## Why

Output was the worst isolated Cycle 34 automation case. A useful replacement
must reduce boundary curvature without trading it for sluggish manipulation.

## Evidence

| Profile | Curvature excess | First-sample motion | Response at 5 ms | Within 1 dB |
|---|---:|---:|---:|---:|
| Current 5 ms | -41.329 dBFS | 0.149689 dB | 63.212% | 17.938 ms |
| One-pole 10 ms | -50.645 dBFS | 0.074921 dB | 39.347% | 35.854 ms |
| One-pole 20 ms | -59.415 dBFS | 0.037479 dB | 22.120% | 71.688 ms |
| Cascade 3+3 ms | -83.271 dBFS | 0.001724 dB | 49.742% | 16.333 ms |

The current lab model matches the actual production render with exactly zero
sample delta. The cascade improves curvature by 41.942 dB, makes the first
sample movement about 86.8 times smaller, and settles 1.604 ms sooner than the
current response. It is the sole retained integration candidate.
Release and sanitizer CSVs are byte-identical with SHA-256
`2e97946ebfe5ad5c2210a8bd4480304446f8f8dfe7e3e286d6d02241fc0f10ba`.
Both full VST3 matrices pass 22/22 tests.

## Risks

The cascade responds 13.470 percentage points less after 5 ms and has not been
auditioned during live control. Measurements cover 48 kHz, one fixture, one
block size, and Output only. Static settings are not evidence for automated
behavior.

## Next step

Integrate the 3+3 ms cascade as the smallest production experiment, then rerun
the complete automation report, golden audio, real-time audit, sample-rate
matrix, and CPU benchmark before accepting the behavior change.
