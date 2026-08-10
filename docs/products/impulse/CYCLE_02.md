# Impulse cycle 02 — eight objects and visible programming

## What changed

Added Low, Crack, Metal, and Cut synthesis voices; replaced hidden Euclidean
playback with eight always-visible 32-step Off/Hit/Accent rows; retained
Pulses/Rotation as an explicit generator; added a selected-object sound editor;
and migrated state to schema 2.

## Why

The four-object palette did not cover the requested instrument range, and a
sequenced instrument without visible event entry was not practically
programmable. The new grid makes playback state direct and inspectable.

## Evidence

- Processor, plugin, editor-artifact, and independent VST3-host tests pass.
- A scheduling test clears every pattern, enables only Kick step 4, and measures
  its first output at the expected sample position.
- All eight MIDI objects, six rates, variable blocks, deterministic seeds,
  zero latency, zero allocation, state fuzzing, and forbidden operations pass.
- Schema 2 round-trips 368 parameters; schema 1 reconstructs the prior four
  Euclidean patterns from old Length/Pulses/Rotation values.
- Five eight-object/all-cell stress renders: 1.23198–1.27919% of one M4 Pro
  core, 1.26032% median at 48 kHz / 127.

## Risks

The 368-parameter host surface is intentionally large and needs automation-list
inspection in Cubase and Ableton. High-frequency objects still need aliasing
listening at 44.1 kHz, and the CPU number needs a five-run median.

## Next step

Program complete rhythms in both target hosts, then evaluate the eight object
ranges in real arrangements before changing synthesis topology.
