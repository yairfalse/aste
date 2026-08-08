# Cycle 18 — Sample-rate and block consistency

## What changed

Added a production-DSP consistency renderer covering all six supported sample
rates and a repeating schedule of all thirteen required nonzero block sizes.
CTest now gates the generated machine-readable CSV report.

## Why

Finite-output unit tests cannot show whether host block partitioning changes the
sound or whether the current detector and nonlinear path drift materially with
sample rate. This measures both on the production signal graph.

## Evidence

- Fixed 127-sample and variable-block renders are sample-identical at every
  tested rate; maximum sample delta is exactly zero.
- From 44.1 through 192 kHz, RMS gain spans 0.014806879 dB, peak spans
  0.000091079 dB, crest factor spans 0.014972501 dB, stereo correlation spans
  0.000032749, and maximum gain reduction spans 0.039808273 dB.
- Latency remains zero samples at every rate.
- Release and ASan/UBSan VST3 suites both pass 8/8.
- Release and sanitizer reports are byte-identical with SHA-256
  `a70df0980f3006e34c12f216723ae170279ed7ac61ee67318e1220aaf15bbc10`.

## Risks

The test uses one synthetic two-second fixture and one production parameter
profile. Global level, dynamics, and stereo metrics do not measure nonlinear
aliasing or prove perceptual equivalence between sample rates.

## Next step

Measure alias energy from the saturation and clipping stages across supported
sample rates before choosing an oversampling strategy.
