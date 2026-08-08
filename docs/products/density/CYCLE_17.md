# Cycle 17 — First production DSP golden

## What changed

Added a dependency-free production golden renderer and tolerance comparator.
Four deterministic stereo fixtures render through the current Density signal
path to Float32 WAVs and a tracked metric manifest.

## Why

The detector research files do not protect the production signal graph. A
reviewed production baseline makes later DSP experimentation safe without
assuming exact sample identity is the only meaningful invariant.

## Evidence

- Transient, bass, dense, and ambient fixtures provide sixteen rendered seconds
  at 48 kHz in non-power-of-two 127-sample blocks.
- The manifest records RMS, peak, crest factor, gain change, stereo correlation,
  maximum gain reduction, latency, and an FNV-1a sample fingerprint.
- A second render reproduces all four WAV SHA-256 values and the manifest
  SHA-256 `6eba96d9d8605741a4329afefdf0596f509dbace9a3e69c82c29188314b8741d`.
- The comparator reports four fixtures within tolerance, zero fingerprint
  changes, and zero-sample latency.
- CTest regenerates build artifacts but cannot modify the tracked baseline.
- Release core tests pass 5/5, Release VST3 tests pass 7/7, and the
  ASan/UBSan VST3 build passes 7/7.

## Risks

This first baseline uses generated material, one parameter profile, one sample
rate, and one block schedule. It is engineering-reviewed but does not replace
the pending blind detector evaluation or listening on real musical projects.
FNV-1a is a change fingerprint, not a security hash or perceptual metric.

## Next step

Measure production-render consistency across all six supported sample rates and
variable block schedules before adding another golden parameter profile.
