# Loop cycle 01 — working VST3 prototype

## What changed

Added the independent Loop L-01 circular-memory DSP, host/MIDI adapter,
19-parameter schema, five presets, scalable single-panel editor, offline
renderer, universal VST3 target, installer, ABI host test, and automated core
and adapter evidence.

## Why

The family needed a playable capture instrument whose varispeed, separate pitch
mechanism, splicing, transport instability, degradation, and amplifier behavior
emerge from one memory architecture.

## Evidence

Core tests cover capture, playback, clear, reverse, speed, pitch, hostile
settings, stereo identity, four sample rates, variable blocks, zero latency,
and zero allocation. Adapter tests cover control-state recall, malformed state,
sample-offset MIDI capture, real-time forbidden operations, meters, UI
visibility, and rendering. The independent host loads the actual VST3 bundle.
The local universal binary is signed, contains arm64 and x86_64 slices, and its
worst-case 48 kHz/127-sample lab run measured a 0.369393% five-run median of one
M4 Pro core (0.366017–0.378431%).

## Risks

Captured memory is not serialized in schema 1. Cubase/Ableton behavior,
transport changes, native Intel performance, pitch artifacts, long overdub
stability, accessibility, and musical usefulness remain external evidence.
The amplifier is a bounded behavioral stage, not a measured hardware model.

## Next step

Run [MUSIC_MACHINE_TEST.md](MUSIC_MACHINE_TEST.md), then design the fixed-memory
snapshot handoff only if host testing confirms the instrument interaction.
