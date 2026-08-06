# Cycle 03 — Continuous stereo linking

## What changed

Added the stable `stereo` parameter to the DSP, state, VST3 automation surface,
and panel. It continuously interpolates each channel detector toward the
greater detector, with independent envelopes and 10 ms smoothing.

## Why

The previous shared envelope was always fully linked. The continuous control
now spans independent, partial, and fully linked behavior without a topology
switch, allowing image stability to be chosen for the programme.

## Evidence

- Endpoint tests prove the dominant channel is unchanged while weak-channel
  reduction increases monotonically at 0%, 50%, and 100% link.
- Identical stereo channels remain sample-identical; processing still performs
  no heap allocation.
- Release and ASan/UBSan core/plugin tests pass.
- pluginval strictness 10 passes all requested sample rates and block sizes.
- Steinberg VST3 SDK 3.8.0 validator passes all 47 tests with 11 parameters.
- Five Release benchmark runs at 48 kHz / 128 samples measured a 0.2083%
  median of one M4 Pro core under block-rate Density/Stereo automation.

## Risks

The CPU result covers one arm64 machine, not the oldest supported Apple Silicon
or Intel targets. Stereo behavior has objective endpoint coverage but still
needs listening on hard-panned percussion, ambience, polarity inversion, and
real mixes. The saturator remains un-oversampled.

## Next step

Implement a slow reference detector comparison in the DSP lab, beginning with
RMS-like peak influence versus the current peak detector, then retain only a
measurably and musically useful alternative.
