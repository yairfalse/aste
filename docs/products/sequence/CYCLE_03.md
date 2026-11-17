# Sequence cycle 3 — polyphonic current

2026-11-15. User-directed sound revision: OB-8-inspired keyboard richness with
TB-style sequencer movement. Original synthesis, no hardware-fidelity claim.

Eight preallocated voices add independent amplifier/filter contours, dual
band-limited oscillators, subtle deterministic tuning/PWM motion, a 12/24 dB
TPT low-pass path and fixed stereo positions. Sustain and three-millisecond
voice-steal transitions support keyboard playing. Mono and host pattern modes
retain slide/accent phrasing. The original engine is still available.

Fresh plugin instances match Warm Brass and play chords without starting the
transport. Six complete snapshots: Warm Brass, Wide Strings, Slow Glass,
Pulse Bass, Acid Current, Original Mono. Voice Mode is visible at minimum size.
Schemas 1–3 select Original mono; schema 3 preserves the corrected clock flag.
The new parameter is appended at host index 85, leaving every old index stable.

## Local evidence

- Universal arm64/x86_64 Sequence VST3 0.4.0 and strict ad-hoc signature verified.
- 68 CTest checks passed, including actual VST3 ABI loading and processing.
- New checks cover simultaneous chord fundamentals, independent note release,
  sample offsets, zero-velocity note-on, pedal release, stealing more than eight
  voices, bounded output at extreme settings over five rates, allocation-free
  rendering, deterministic reset and partition-independent mono sequencing.
- Adapter checks cover schema-3 original-voice migration, schema-4 round trips,
  stable host parameter indices, preset recognition and visible mode selection.
- Core AddressSanitizer and UndefinedBehaviorSanitizer tests passed.
- A side-by-side compile of the prior engine and Original mono produced 508,000
  bit-identical samples under pressure/cutoff automation in both timing modes.
- Eight held voices at 48 kHz / 127 frames measured 3.16% of one core in a short
  local Release benchmark; this is not an oldest-supported-machine CPU budget.
- Five 8-second stereo, 24-bit, 48 kHz WAVs rendered through the actual plugin
  presets. Peak amplitudes ranged 0.313–0.541 with finite samples.
- Minimum-size editor inspected; default identifies Warm Brass correctly.

The local build contains listening WAVs in `sequence-listening/`, editor PNGs
in `sequence_editor_artifacts/`, and temporary compatibility/benchmark tools.
Recreate WAVs with `sequence_plugin_tests --listening-artifacts <directory>`.
These artifacts are ignored build products, not release evidence archives.

## Limits and next listening gate

The musician still needs to judge warmth, brightness, chord balance, mono bass
weight, accent/slide expression and stereo-to-mono balance in Cubase. There is
no listening approval or claim of matching an OB-8. Filter Weight is a continuous
12/24 dB blend, not a hardware switch. Mode changes clear held voices. Sustain
is channel-agnostic; no MPE, unison stack, chorus, oversampling or new sequencer
features were added. The five other instruments have no DSP changes. A family
version bump identifies the source; only Sequence is locally redeployed here.
