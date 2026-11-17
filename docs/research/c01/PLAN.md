# C-01 MVP and remaining decisions

## Current MVP

A separate development VST3 instrument, 12 individual MIDI voices, sample-rate-dependent
internal generation around 176–192 kHz, coupled phase core, correlated feedback contamination, relaxing
hybrid operating memory, five gestures plus Colour/ADSR, velocity/CC1/channel
pressure/sustain/pitch wheel, strict scalar recall, host-notifying starting
points, state clear/panic, native single-panel UI and measured readouts.
C++20 has no JUCE dependency; adapter uses the existing Aste experimental
instrument and parameter conventions. Isolated build has no release installer
integration. The generated bundle is local and is not installed.

## Implementation sequence after this cycle

1. Listen/rate C1–C8 and C10 without first reading the key. Select or reject the
   reciprocal phase candidate against the filter-return alternative. Adjust
   shared/local memory time constants based on audible causality and sparse
   playing, not only first/last waveform differences.
2. Establish moderate Pressure weight at matched perceived loudness. If it only
   brightens/thins the tone, revise the operating path before adding controls.
   Check upper register, low clustered chords, minimum/maximum envelope times,
   and 12 overlapping voices. Compare against improved Sequence/Field patches.
3. Measure reference-resampled nonlinear alias residuals, sweeps and moving
   controls; review the 176–192 kHz internal-rate policy against higher factors or a reduced-bandwidth high-note mapping. Consider
   stateful antialiasing only with topology/delay validation. Profile per-voice
   math and coefficient work if the measured 96 kHz budget is unacceptable.
4. Evaluate C9 with a musician; inspect real Cubase loading, automation ramps,
   bypass, transport stop, seeks, sustain/stealing, preset changes and saved
   sessions. Test VST3 controller/component state round trips, mono/stereo,
   multiple instances, offline bounce and host block-size changes. Run a VST3
   validator. Current JUCE/plugin smoke tests are not Cubase certification.
5. Add accepted DSP/UI regression fixtures and adversarial parameter/MIDI
   testing around findings; run long-session denormal/CPU stress and host reload
   tests. Run native VoiceOver/HiDPI/contrast review. Document accepted host
   compatibility and CPU targets, then integrate a dedicated opt-in root target
   and packaging. Keep existing products' identities/ordering unchanged.
6. Only after listening acceptance, freeze product response version, factory
   presets and release documentation; build/sign the required universal binary
   and review installation as a concrete artifact. Current build is native arm64.

## Unresolved questions

- Does the coupled pair justify its own instrument, especially against current
  Sequence Poly? Are its dark dry chords actually compelling?
- Is operating memory perceptually meaningful across sparse events, rather than
  merely measurable? Should memory follow a slot, pitch or note identity?
- Does the small shared rectified-band reservoir provide enough chord-dependent
  interaction, or is a carefully controlled shared audio path needed?
- Does Instability need actual interacting resonant transitions beyond the
  current correlated drift plus nonlinear phase return? No chaos claim yet.
- Is 4× sufficient at high pitch/high Pressure? Off-harmonic leakage is a proxy;
  8× improves the worst measured high note at a substantial CPU cost.
- Should a stopped-transport clear be configurable? Should full live memory ever
  be checkpointed? Current scalar recall always begins empty, deliberately.
- Are fixed ±2-semitone bend and channel pressure sufficient? MPE, poly pressure,
  MIDI learn, preset file import/export and modulation routing are not MVP features.
- Can the actual state strip communicate more clearly as calibrated bars?
  Numeric observations currently avoid implying an unimplemented spectrum.
