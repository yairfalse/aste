# Field F-01 — cycle 01

## What changed

Added the sixth independent product: C++20 spatial-field DSP, nine-parameter
VST3 adapter, sample-offset MIDI excitation, versioned state, five factory
starting points, scalable industrial editor, offline report/benchmark, core and
adapter tests, ABI smoke-host coverage, macOS installer, and CI gates.

## Why

A conventional reverb topology would not deliver the requested playable
instrument. The eight-line field makes stored energy the source of grain,
motion, pitch accumulation, and stereo width. FOREVER provides the requested
single-button entry point while six descriptive sound controls retain useful
depth.

## Evidence

- strict-warning Release build and Clang static analysis pass;
- core properties pass at 44.1–192 kHz with non-finite controls;
- output is deterministic across 1/2/7/127/511/2048-sample partitions;
- callback allocation count is zero;
- macOS adapter audit observes zero locks, file opens, and direct writes;
- deterministic state round-trip and malformed-state rejection pass;
- the signed VST3 loads through the independent smoke host with 9 parameters,
  0 samples latency, 680 state bytes, irregular blocks, and finite output;
- five local 48 kHz / 127-sample worst-case runs measure a 0.320727% median
  (0.320444–0.323781%) of one Apple M4 Pro performance core;
- processing state is 1,180,056 bytes, below the explicit 1.5 MiB Field budget.

## Risks

The sound has synthetic-fixture evidence but no musical or real-DAW verdict.
Live field memory is not serialized. The fixed +7/+12 pitch architecture may be
too recognizable or too bright on dense input. The 1.5 MiB exception exceeds
the general 256 KiB non-memory-effect budget and still needs native Intel and
oldest-supported Apple Silicon measurements.

## Next step

Install the universal bundle on the music Mac and execute
`MUSIC_MACHINE_TEST.md`, beginning with FOREVER stability, grain click behavior,
and pitched-feedback usefulness on voice, guitar, and sparse synthesizer.
