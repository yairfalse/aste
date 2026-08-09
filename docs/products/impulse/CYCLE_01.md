# Impulse cycle 01 — working VST3 prototype

## What changed

Added the independent four-object synthesis engine, sample-position host clock,
polymetric Euclidean scheduler, seeded probability/mutation, MIDI excitation,
60-parameter state boundary, five snapshots, scalable editor, DSP lab, universal
VST3 target, installer, ABI host support, and automated tests.

## Why

The family required a rhythm instrument built from physical signal events and
long interacting cycles rather than prerecorded genre kits or a conventional
sixteen-step drum-machine identity.

## Evidence

Core tests prove deterministic seed behavior, exact output across variable
block partitions, different output for different seeds, six sample rates,
sample-offset MIDI triggering, finite hostile settings, zero allocation, and
zero latency. Adapter tests cover all 60 parameters, seed/pattern state recall,
malformed state, callback allocations/locks/files/writes, stereo output, UI
visibility, and the independently loaded VST3 ABI. The universal artifact is
ad-hoc signed and contains arm64 and x86_64 slices. Five worst-case 48 kHz/127
sample benchmark runs measured a 0.352056% median of one M4 Pro core
(0.339692–0.367345%).

## Risks

Cubase/Ableton transport, host-loop boundaries, native Intel performance,
high-density aliasing, sub translation, long-cycle musical usefulness,
accessibility, and real-project behavior remain external. Voices are
behaviorally designed; no measured-hardware or exact-emulation claim exists.

## Next step

Run [MUSIC_MACHINE_TEST.md](MUSIC_MACHINE_TEST.md), prioritizing restart/locate,
15/23/11/16 realignment, deep-kick phase, and deterministic bounce comparison.
