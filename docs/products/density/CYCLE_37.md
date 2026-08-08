# Cycle 37 — Drive smoothing comparison

## What changed

Added a lab-only full-graph comparison of the current 5 ms Drive smoother, 10
and 20 ms one-pole variants, and a two-stage 3+3 ms exponential response.
Production Drive behavior is unchanged.

## Why

Drive became the worst isolated automation case after the Output correction.
It cannot be evaluated as post-processing gain because it feeds both crushed
audio and the detector.

## Evidence

| Profile | Curvature excess | First-sample motion | Response at 5 ms | Within 1 dB |
|---|---:|---:|---:|---:|
| Current 5 ms | -48.366 dBFS | 0.149689 dB | 63.212% | 17.938 ms |
| One-pole 10 ms | -54.617 dBFS | 0.074922 dB | 39.347% | 35.854 ms |
| One-pole 20 ms | -60.411 dBFS | 0.037480 dB | 22.120% | 71.688 ms |
| Cascade 3+3 ms | -79.139 dBFS | 0.001724 dB | 49.742% | 16.333 ms |

The current lab profile matches production with exactly zero sample delta. The
cascade improves curvature by 30.774 dB, makes first-sample movement about 86.8
times smaller, and settles 1.604 ms sooner. The 20 ms profile passes the
provisional ceiling by only 0.411 dB while settling four times slower, so it is
rejected. The cascade is the sole retained integration candidate.
All three CSVs are byte-identical with SHA-256
`2fd1f339bc6ee49eaa803d14a15746eb30d8c631cd650492e833e474f9170b2b`.
Production golden fingerprints remain unchanged, and full Release and
Address/UndefinedBehavior sanitizer matrices pass 23/23 tests, including the
callback-safety audit.

## Risks

Drive changes detector history and nonlinear level, so one synthetic fixture
cannot establish behavior on real programmes. The candidate has not been
auditioned during live manipulation and has only been measured at 48 kHz with
127-sample automation blocks.

## Next step

Integrate the Drive 3+3 ms cascade, then rerun the complete automation, golden,
sample-rate, CPU, callback-safety, and validator matrices before accepting it.
