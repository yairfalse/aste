# Sequence cycle 01 — working VST3 prototype

## What changed

Added the independent Sequence S-01 monophonic synthesizer core, 16-step
host-synchronised program, 83-parameter state boundary, scalable single-panel
editor, universal VST3 target, installer, ABI smoke-host support, and automated
core/adapter tests.

## Why

The new family member needs the immediacy of a programmed bass instrument with
a broader original voice and easier visible programming, without copying a
commercial circuit, product name, or panel.

## Evidence

Core tests cover six sample rates, irregular and variable blocks, deterministic
reset, MIDI, PPQ selection, finite hostile parameters, monotonic Pressure, zero
latency, and zero processing allocation. Adapter tests cover all 83 parameters,
state/fuzz behavior, mono/stereo output, MIDI synthesis, bypass, meters, editor
visibility/scaling, and the real VST3 ABI. The universal bundle passes strict
code-signature and arm64/x86_64 slice checks.

## Risks

No Cubase/Ableton run, native Intel performance measurement, musical range
review, high-resonance listening, validator run, accessibility review, or
notarized distribution evidence exists yet. The ladder is behaviorally
informed, not a component model.

## Next step

Run [MUSIC_MACHINE_TEST.md](MUSIC_MACHINE_TEST.md), starting with transport
restart/loop behavior and level-matched filter-form listening.
