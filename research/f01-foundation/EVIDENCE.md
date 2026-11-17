# Actual measurements — Field foundation

2026-11-17, local macOS arm64, Release C++20; no production DSP replacement.

## Decay and stereo

Nominal 2.4 s internal setting; commercial reference defaults are measured as-is,
not adjusted to an identical RT. Values below use the same .25 dry impulse,
48 kHz, two output channels. RT60 is **T20 extrapolation**, fitted between -5 and
-25 dB of backward-integrated energy. It is not a measured full 60 dB decay.
Band filters are causal RBJ Butterworth high/low pairs at octave edges, evaluated
on a zero-padded FFT grid. They are approximate octave-like bands, not certified
IEC octave filters. Fit R² and rejected/truncated ranges are retained in CSV.

| Engine | Broadband s | 125 Hz s | 8 kHz s | L/R correlation, .1–2 s |
|---|---:|---:|---:|---:|
| Field basic | 2.134 | 2.554 | .991 | .059 |
| Field default features | .727 | .731 | .555 | .157 |
| Bare8 | 1.887 | 2.384 | 1.287 | .012 |
| House16 | 1.824 | 2.445 | 1.338 | -.017 |
| Hadamard16 | 1.831 | 2.400 | 1.319 | -.049 |
| Schroeder | 1.742 | 2.396 | 1.582 | -.008 |
| Cross-tank | 2.068 | 2.579 | 1.777 | -.023 |
| Pro-R | 2.103 | 2.787 | 1.786 | -.007 |
| UADx 224 | 2.053 | 3.239 | 1.455 | .004 |

These are responses to a particular impulse at a particular modulation phase.
Time-varying and nonlinear systems do not have one invariant LTI impulse
response. Correlation is an uncentered normalized dot product in this window;
it does not prove spaciousness. Mono energy fractions and early energy ratios
are also recorded, rather than judging stereo quality from correlation alone.
The first-arrival detector ignores residual numerical dry leakage below the
larger of 1e-8 and -80 dB relative to the response peak.

The internal raw 20-second IR set also tests nominal .6 and 8 s. Field's gain
floor prevents the .6 s request from being reached; the request is a fixture
label, not a claimed physical decay. Do not interpret the shortest-case labels
as decay-matched comparisons. Check long-tail fits for finite-record truncation.

## Early echo-density proxy

Twenty-millisecond left-channel windows, local mean removed. Count samples
exceeding local RMS and divide by the Gaussian expectation .31731. NED near 1
means Gaussian-like amplitude occupancy; it does not directly measure reflection
count, smoothness or audible quality. Sparse periodic signals and intentional
early reflections can defeat this proxy. It can exceed 1.

| Engine | 50 ms | 100 ms | 200 ms |
|---|---:|---:|---:|
| Field basic | .062 | .164 | .529 |
| Field default features | .092 | .358 | .732 |
| Bare8 | .026 | .154 | .446 |
| House16 | .719 | .903 | .916 |
| Hadamard16 | .739 | .834 | 1.067 |
| Schroeder | .699 | .808 | .860 |
| Cross-tank | .732 | .880 | .942 |
| Pro-R | .341 | .469 | .578 |
| UADx 224 | .706 | .305 | .716 |

The neutral Field onset is measurably sparse. The diffused alternatives improve
that property. Pro-R's lower early proxy demonstrates why this is not a quality
ranking. No conclusion that the prototypes outperform the references follows.
The first substantial Field-basic arrival is about 31.1 ms; diffused candidates'
explicit early taps start around 7.1 ms. Their early/late balances also differ,
so the result cannot be attributed to the mixing matrix alone. House16 versus
Hadamard16 is the controlled matrix-only comparison.

## Performance and correctness

- Release contracts: 1/1 passed in 3.05 s. Five candidates, six rates:
  44.1/48/88.2/96/176.4/192 kHz. Exact equality for 127 versus 511 frame partitions
  with automation at the same sample, and 1 versus 127 frames at 48 kHz.
- Extremes: decay .15–20 s, high ratio .15–1, diffusion 0–.78, modulation 0–1,
  abrupt parameter changes, NaN/Inf controls/input recovery, silence and reset.
- Each candidate also processes 30 seconds at maximum decay/diffusion/movement,
  with eight seconds of hot sustained tone plus DC followed by silence.
  Tested output remains finite and below the test's 32-unit bound. Detailed
  peaks are in the test log. This is stability evidence, not 0 dBFS normalization.
- Global new/delete audit and calibrated macOS mutex/file-open/write interposition
  detect no calls during process. Prepare/reset lifecycle operations are outside
  this callback audit. No threads or waiting primitives exist in these cores.
- Final separate ASan/UBSan test build: 1/1 passed in 69.70 s.
- Existing production code is frozen, not rewritten. Current Field reference
  callback is rendered separately; new-engine RT audits do not retroactively
  certify all operations of the existing product.

Benchmark: 128 frames, 100 warm-up + 300 timed blocks per engine/rate. Candidate
controls are maximum decay, strongest damping, maximum diffusion and movement.
Old-engine cases use their basic/default-feature settings. This is not identical
parameter semantics, and measurements are a local budget, not a DAW guarantee.

| Engine at 48 kHz | Median µs | p99 µs | Max µs | Allocated bytes |
|---|---:|---:|---:|---:|
| Field basic | 28.542 | 36.125 | 41.541 | 4,252,480 |
| Field default features | 44.000 | 47.083 | 50.500 | 4,252,480 |
| Bare8 | 15.750 | 16.292 | 16.625 | 549,640 |
| House16 | 29.708 | 41.750 | 132.333 | 549,640 |
| Hadamard16 | 29.750 | 33.500 | 36.209 | 549,640 |
| Schroeder | 16.459 | 17.708 | 24.167 | 549,640 |
| Cross-tank | 10.875 | 11.084 | 11.125 | 662,216 |

The 48 kHz block deadline is 2666.7 µs. House16 has an observed scheduling outlier;
it has not been discarded. Full CSV includes 44.1/48/96/192 kHz, and memory
scales with rate. Candidate objects retain unused alternative delay allocations,
so these are actual allocated sizes of the lab, not optimized product budgets.
Current Field includes its two-bank loop memory even when it is unused. No
commercial-plugin benchmark or hard real-time host guarantee is claimed.

## Reproducibility and rejected passes

- `final/`: 61 internal WAVs, raw IRs, dry fixtures, levels, band/decay/density CSVs,
  decay JSON/SVG, listening HTML, SHA256 manifest.
- `reference-comparison/`: 45 nine-way wet comparisons, common target per source,
  <=.05 LU matching error, peaks <=.8; generated after warmed external renders.
- `reference-ir/`: nine same-input IRs, band/decay/spatial/density measurements.
- `pro-r-warmed/`, `uad-224-warmed/`: actual raw commercial renders; logs retain
  parameter enumeration and component versions. Their defaults, and thus decay
  curves, are not identical to the candidate settings.
- Earlier `listening*` folders contain single-pass gain matching or pre-final
  initialization behavior; excluded from the final recommendation.
- `*-raw/` commercial folders are unprimed passes. UAD's first impulse failed to
  excite normally, so that pass is excluded. A 24,576-frame silent warm-up solved
  the observed startup issue before the identical input.

No human listening, Cubase retry, subjective winner, flutter/metallicity verdict,
production integration or successful FOREVER redesign is claimed. These are
explicitly open evaluation steps. The next action is listening to the prepared
pack and choosing whether an architecture merits further conventional-reverb
refinement. Additional experimental transforms must wait.
