# Cycle 34 — Automation discontinuity measurement

## What changed

Added one deterministic DSP-lab report and CTest covering abrupt endpoint
automation for every continuous parameter, both separately and simultaneously.
Production DSP is unchanged.

## Why

Validator automation proves host correctness, not absence of audible zipper
noise. Boundary curvature supplies a repeatable signal-level measurement before
changing smoothing behavior.

## Evidence

| Case | Maximum curvature excess | Provisional -60 dBFS gate |
|---|---:|---|
| Drive | -48.366 dBFS | Fail |
| Crush | -66.767 dBFS | Pass |
| Attack | -53.086 dBFS | Fail |
| Release | -76.340 dBFS | Pass |
| Density | -70.858 dBFS | Pass |
| Blend | -56.401 dBFS | Fail |
| Stereo | -79.459 dBFS | Pass |
| Output | -41.329 dBFS | Fail |
| Detector HPF | -68.701 dBFS | Pass |
| All simultaneously | -38.175 dBFS | Fail |

All 3,770 measured transitions and output samples remain finite. The report
distinguishes `measurement_valid` from `within_ceiling`; a valid failing result
still allows regression testing without weakening the quality gate.
Release and sanitizer reports are byte-identical with SHA-256
`185b4473fa0a4e4b8a68f7acd17021fd585f35c5893e5de3377f2fd54b6b5720`.
Both full VST3 matrices pass 21/21 tests.

## Risks

The -60 dBFS ceiling is provisional, not a psychoacoustic threshold. This test
uses one synthetic stereo signal at 48 kHz and 127-sample automation blocks;
boundary curvature indicates sudden trajectory changes but does not alone prove
audibility.

## Next step

Isolate Output, the worst single parameter, and compare minimal smoothing
responses against both curvature and manipulation lag before changing the
production smoother.
