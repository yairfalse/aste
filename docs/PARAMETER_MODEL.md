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
| `attack` | Attack | 0.02..30 ms | 1 | logarithmic | 5 ms coefficient space; legacy: log-time |
| `release` | Release | 20..1200 ms | 180 | logarithmic | coefficient per block |
| `density` | Density | 0..100 % | 50 | linear | 10 ms |
| `blend` | Blend | 0..100 % | 90 | linear | cascaded 3+3 ms |
| `stereo` | Stereo link | 0..100 % | 100 | linear | 10 ms |
| `output` | Output | -24..12 dB | -7 | linear dB | cascaded 3+3 ms |
| `detector_hpf` | Detector HPF | 20..300 Hz | 90 | logarithmic | coefficient per block |
| `protection` | Protection | off/on | on | boolean | deliberate transition |
| `bypass` | Host bypass | off/on | off | boolean | host transition |

Drag, fine adjustment, reset, typed entry, and text conversion belong to the
JUCE adapter. Exact parsing rejects trailing junk and non-finite values.

Density continuously lowers threshold from -18 to -46 dB while increasing ratio
from 3:1 to 60:1, saturation drive from 1 to 9, release curvature from 1 to 5,
and crushed-path make-up from 12 to 22 dB. Crush scales the nonlinear
participation as well as the gain computer. Positive Drive raises saturation
drive by up to 75%, so it remains a timbral gesture after compression. There
are no topology switches. `mapDensity()` and `mapCrushSaturation()` are the
maintainer-visible mappings and have monotonicity and boundary tests. The lower
threshold engages ordinary track levels; additional crushed-path make-up keeps
that branch present in the blend. Output remains an independent level trim.
At the default Density/Crush settings the effective threshold is -27.8 dBFS.

## Factory starting points

The compact `PRESETS` action menu applies six product-local parameter snapshots
through normal host-notifying parameter updates. The original five select legacy
behavior; appended `Grip` selects the new direct-Drive behavior and is the new
instance starting point. The table above remains the host reset defaults, not
the initial preset. Schema 2 stores behavior separately from stable parameter
values; see [STATE_FORMAT.md](STATE_FORMAT.md). Protection is on and bypass is
off in every snapshot.

| Name | Drive | Crush | Attack | Release | Density | Blend | Output | Stereo | HPF |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| Default | 0 dB | 65% | 1 ms | 180 ms | 50% | 90% | -7 dB | 100% | 90 Hz |
| Continuity | 2 dB | 50% | 8 ms | 320 ms | 35% | 50% | -3 dB | 100% | 120 Hz |
| Proximity | 6 dB | 75% | 0.5 ms | 140 ms | 70% | 80% | -5 dB | 85% | 100 Hz |
| Parallel Crush | 12 dB | 95% | 0.05 ms | 80 ms | 90% | 50% | -8 dB | 100% | 140 Hz |
| Transient Hold | 4 dB | 85% | 6 ms | 500 ms | 60% | 70% | -4 dB | 65% | 180 Hz |
| Grip | 6 dB | 85% | 6 ms | 80 ms | 65% | 100% | -9 dB | 100% | 90 Hz |

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

Sequence schema 2 contains 21 voice/clock controls and four stable controls for
each of 16 visible steps, for 85 parameters total. Host MIDI-CC emulation is
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

Impulse schema 2 contains eight global parameters, thirteen sound/cycle
parameters for each of eight objects, and 32 tri-state pattern cells for each
object, for 368 stable IDs. Seed, cycles, probability, conditions, ratchets,
timing, and the complete visible program are ordinary scalar state. The
complete contract is
[docs/products/impulse/PARAMETERS.md](products/impulse/PARAMETERS.md).

## Field F-01

Field schema 1 contains nine stable IDs: Forever, Mass, Grain, Pitch, Motion,
Distance, Blend, Output, and Bypass. The single hero button changes retention
continuously; it is serialized like any other automatable parameter. Complete
ranges, defaults, smoothing, and internal coordinated mappings are in
[docs/products/field/PARAMETERS.md](products/field/PARAMETERS.md).

## Final master level (0.5.0)

`master` is appended after the existing host parameters, with -36..+18 dB
range and 20 ms smoothing. It acts after sound shaping and the existing Output
control. Output meters report this final level. Its header control is always
visible, with exact text entry and Shift-drag fine changes. Old sessions without
this parameter restore 0 dB. Fresh instances start at 0 dB.
The original parameter indices, ranges and existing Output behavior remain.

Density 0.6.2 appends `character` after Master: 0..100%, reset default 0,
new Bite / breathing preset 80. It blends a DC-filtered asymmetric saturating
branch and changes level-dependent release recovery. Field appends `focus`
after Texture (0..100%, reset 0; Moving glass 75). Both missing IDs migrate to
zero in schema 1/2 states, independently of the receiving instance's preset.
Existing host parameter ordering and identities remain intact.

0.7.0: Non-Machine appends fifteen voice material parameters after its previous
409 host slots. Field appends seven nested-memory parameters after its previous
12 slots. See their product documents for IDs and units. Original sources /
no captured-memory mix are the missing-parameter migration defaults. No existing
parameter identity is changed; no live audio buffer is serialized.

## Material response (0.8.0)

Density, Harmonic, Sequence, Impulse, Field, Non-Machine and Resonator save a
root `material` attribute: absent/0 retains the pre-0.8.0 response; 1 selects
updated material behavior. Malformed values reject the restore transaction.
Fresh instances use 1. USE UPDATED SOUND adopts it without changing parameter
values; this is session response metadata, not an automatable parameter.
Loop's revision adds a factory preset only. Existing schema numbers remain.

N-01 appends `v1_e2_sound_lock` through `v3_e8_sound_lock` (event-major within
each voice, skipping base event 1), 21 boolean controls at indices 424–444.
All default off. Updated unlocked hits derive Level/Pitch/Decay/Color from
the object's first event; articulation and timing remain per-event. GUI lock
enabling copies the current object sound before setting the lock. Automation
uses the stored hit values directly. Older-response sessions ignore these locks.

## Click K-01

K-01 schema 1 exposes 2592 stable parameters: 22 globals, 34 fields for each
of four objects, 19 locks/articulation fields for each of 128 events, then Master
and Bypass. All object/event host titles identify their address. Sample bounds
remain normalized host values; editor entry shows frames and also accepts ms.
Absolute sample references and four scene snapshots are metadata; live PCM and
master captures are not serialized. See [the contract](products/click/CONTRACT.md).
