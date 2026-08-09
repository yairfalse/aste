# Parameter model

Stable IDs use product-local ASCII names. Normalized host values map to physical
ranges; ranges are provisional until musical validation, but IDs are treated as
permanent from the first external build. Products use the shared strict
decimal parser for text entry while retaining independent parameter layouts.

## Density D-01

| ID | Name | Unit/range | Default | Mapping | Smoothing |
|---|---|---|---:|---|---|
| `drive` | Drive | -12..24 dB | 0 | linear dB | cascaded 3+3 ms |
| `crush` | Crush | 0..100 % | 65 | linear | 10 ms |
| `attack` | Attack | 0.02..30 ms | 1 | logarithmic | 5 ms in log-time space |
| `release` | Release | 20..1200 ms | 180 | logarithmic | coefficient per block |
| `density` | Density | 0..100 % | 50 | linear | 10 ms |
| `blend` | Blend | 0..100 % | 50 | linear | cascaded 3+3 ms |
| `stereo` | Stereo link | 0..100 % | 100 | linear | 10 ms |
| `output` | Output | -24..12 dB | 0 | linear dB | cascaded 3+3 ms |
| `detector_hpf` | Detector HPF | 20..300 Hz | 90 | logarithmic | coefficient per block |
| `protection` | Protection | off/on | on | boolean | deliberate transition |
| `bypass` | Host bypass | off/on | off | boolean | host transition |

Drag, fine adjustment, reset, typed entry, and text conversion belong to the
JUCE adapter. Exact parsing rejects trailing junk and non-finite values.

Density continuously lowers threshold while increasing ratio, saturation drive,
program-dependent release curvature, and crushed-path make-up. There are no
topology switches. `mapDensity()` is the maintainer-visible mapping and has a
monotonicity test.

## Factory starting points

The compact `PRESETS` action menu applies five product-local parameter snapshots
through normal host-notifying parameter updates. Presets add no state field or
binary format: the resulting stable parameter values serialize through schema 1
like any manual setting. Protection is on and bypass is off in every snapshot.

| Name | Drive | Crush | Attack | Release | Density | Blend | Output | Stereo | HPF |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| Default | 0 dB | 65% | 1 ms | 180 ms | 50% | 50% | 0 dB | 100% | 90 Hz |
| Continuity | 2 dB | 50% | 8 ms | 320 ms | 35% | 35% | -1 dB | 100% | 120 Hz |
| Proximity | 6 dB | 75% | 0.5 ms | 140 ms | 70% | 55% | -3 dB | 85% | 100 Hz |
| Parallel Crush | 12 dB | 95% | 0.05 ms | 80 ms | 90% | 32% | -6 dB | 100% | 140 Hz |
| Transient Hold | 4 dB | 85% | 6 ms | 500 ms | 60% | 45% | -2 dB | 65% | 180 Hz |

These are internal-beta starting points, not yet listening-approved factory
content. Their names and values may change before the first external release;
stable parameter identifiers and state compatibility may not.

## Harmonic H-01

Harmonic schema 1 contains 12 stable IDs: Input; gain and frequency for
Foundation, Body, Presence, and Air; Harmonic; Output; and Bypass. Band gains
span -12..+12 dB. Each frequency has a constrained product-specific range, and
the global Harmonic macro spans 0..100%. Gain, coefficient, macro, and
input/output transitions are smoothed without topology changes.

The complete ranges, defaults, text behavior, automation contract, and
maintainer-visible macro mapping are in
[docs/products/harmonic/PARAMETERS.md](products/harmonic/PARAMETERS.md).

## Sequence S-01

Sequence schema 1 contains 19 voice/clock controls and four stable controls for
each of 16 visible steps, for 83 parameters total. Host MIDI-CC emulation is
disabled so the VST3 publishes only this intentional contract. Complete ranges,
defaults, smoothing, step IDs, and the Pressure mapping are in
[docs/products/sequence/PARAMETERS.md](products/sequence/PARAMETERS.md).

## Loop L-01

Loop schema 2 contains 20 stable product parameters; schema 1 migrates with the
new Tape Speed parameter at its default. Synced beat length and free seconds are
different IDs so unit meaning never changes with a mode switch. RELOOP and
generation navigation are bounded actions rather than fake continuous
parameters. Complete ranges, defaults, and musical roles are in
[docs/products/loop/PARAMETERS.md](products/loop/PARAMETERS.md).

## Impulse I-01

Impulse schema 1 contains eight global parameters plus thirteen parameters for
each of four rhythmic objects, for 60 stable IDs. Seed, cycles, probability,
conditions, ratchets, and timing are ordinary scalar state rather than hidden
sequencer data. The complete contract is
[docs/products/impulse/PARAMETERS.md](products/impulse/PARAMETERS.md).
