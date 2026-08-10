# Sequence cycle 02 — character voice correction

## What changed

Replaced the parallel two-filter crossfade with one shared-state nonlinear
character filter. Added Pulse Width and Filter Drive, extended Wave through
sine, migrated state to schema 2, and revised the panel hierarchy and presets.

## Why

Parallel filter outputs could lose body at intermediate Form positions. The new
Weight control exposes one continuous musical phenomenon and the two added
controls make the oscillator/filter interaction directly playable.

## Evidence

- Processor, plugin, editor-artifact, and independent VST3-host tests pass.
- The 0/50/100% Weight render is finite, audible, materially different at its
  endpoints, and has no destructive midpoint RMS collapse.
- Schema 1 restores with 50% Pulse Width and 25% Filter Drive.
- Five Release worst-case renders: 0.288956–0.303562% of one M4 Pro core,
  0.292424% median at 48 kHz / 127.
- Processing remains zero-latency and allocation-free.

## Risks

The new response still needs level-matched selection in Cubase and Ableton on
bass, lead, and percussive programs. No aliasing claim is made for extreme
Filter Drive at 44.1 kHz.

## Next step

Run the revised music-machine protocol and adjust ranges only from recorded,
level-matched observations.
