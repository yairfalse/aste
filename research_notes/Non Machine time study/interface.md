# N-01 interface, source engines and duplication

## What earns a separate instrument?

### Takeaway

Independent cycles, conditions, microtiming and reproducible variation do not
distinguish N-01 from Impulse. The candidate distinction is direct composition
with captured fragments at three temporal levels, explicit return semantics and
structural silence; this remains a hypothesis until comparative listening and
editing trials.

### Cited Findings

The sources below were read in the checkout at base commit
`8a603dcbc59b52a58637fe6305676efc5a406f80`, on 2026-11-14.

| Requirement | Existing implementation | N-01 consequence |
|---|---|---|
| Independent cycles, probability, conditions, timing | Impulse has eight 1–32-step rows, per-track probability, 1–4 ratchets, ±49% track timing and every-N-cycle conditions. [Contract](../../docs/products/impulse/PARAMETERS.md) | Do not present these alone as new. Investigate per-event rather than track-wide edits. |
| Repeatable variation | Impulse hashes stored seed, track and absolute timing position. [DSP](../../apps/impulse/impulse_processor.cpp) | Reuse the pure-identity approach, not a second stateful random engine. |
| Parameter locks | Impulse exposes track sound parameters and Off/Hit/Accent cells, not a separate sound-parameter snapshot per event. [Contract](../../docs/products/impulse/PARAMETERS.md) | Explicit event snapshots are a meaningful experiment, not yet proof of musical advantage. |
| Captured audio, reverse and splice | Loop has three bounded tape decks, varispeed/reverse, duration-preserving pitch and generational printing. [Specification](../../docs/products/loop/SPECIFICATION.md) | Compare against Impulse feeding Loop before adding equivalent controls. |
| Sample-offset performance actions | Loop delivers MIDI capture/reprint actions at event offsets; UI RELOOP/Previous/Next/Clear are bounded requests rather than continuous parameters. [Contract](../../docs/products/loop/PARAMETERS.md) | Gesture recording needs timestamped semantic actions, not sampled widget values. |
| Shared phrase/time ownership | Impulse and Loop own different transport contracts, with extraction explicitly deferred. [Architecture](../../docs/ARCHITECTURE.md) | An offline comparison harness may connect their outputs, but N-01 must not link production processors together. |
| Captured-memory recall | Loop schema 3 retains captured audio; its spec is newer than the schema-2 wording in portions of the architecture/parameter docs. [Specification](../../docs/products/loop/SPECIFICATION.md), [RT rules](../../docs/REALTIME_SAFETY.md) | Do not copy older control-only state assumptions or silently repair unrelated documents. |

### Inferences

Compare three equal-duration tasks, retaining exact edit scripts and listening
responses: (A) Impulse alone with sparse polymeter; (B) Impulse through Loop with
one selected phrase capture; (C) N-01 with a local repeat nested under a master
hold and an explicit silent interval. Count edits and whether a listener can
predict the return; report sound preference separately. Host bounce/editing is
another strong baseline, even if it is slower to perform live.

A minimally differentiated instrument has three objects, per-event snapshots,
one local fragment per object, one master fragment and two named time modes.
This is a cap, not a demand to fill every slot. It needs one reliable route back
to the phrase more than it needs a larger synthesis palette.

### Gaps

No human listening, Cubase session, editing-time measurement, Elektron hardware
evaluation or artist interview was performed for these notes. Musical references
are the user's conceptual brief, not empirical evidence about those artists'
methods. No claim of algorithmic or sonic equivalence is supported.

## Which source engines are worth investigating?

### Takeaway

Pressure, resonant impact and shaped noise are enough to test time manipulation.
Microscopic fragments should initially be another playback mode of captured
material, not a fourth elaborate synthesizer.

### Cited Findings

Impulse already implements sine-based low events with pitch envelopes,
inharmonic sine combinations for body/metal, shaped pulse/cut sounds, filtered
noise and bounded nonlinear loading. Its trigger routine snapshots pitch,
decay, tone, drive, level and pan into a voice. [DSP source](../../apps/impulse/impulse_processor.cpp)

Loop already supports interpolated captured-audio playback, reversal, dual-head
pitch shifting and splice smoothing. Its purpose is cumulative tape-generation
transformation, rather than event-level scheduling. [DSP source](../../apps/loop/loop_processor.cpp),
[Specification](../../docs/products/loop/SPECIFICATION.md)

The repository permits sharing only after a second identical consumer or a
correctness requirement, and prohibits product processors linking to each
other. [Architecture](../../docs/ARCHITECTURE.md)

### Inferences

| Candidate | Small experiment | Alternative / risk | Relationship to Impulse |
|---|---|---|---|
| Pressure | Short low sinusoidal body with shaped onset and finite decay; avoid a compulsory kick-like pitch sweep | Noise-driven low resonator gives less predictable pitch; sub-energy can consume headroom without audible detail | High duplication; use as a timing reference, not N-01's identity |
| Impact | A few inharmonic damped modes with per-event energy and damping snapshots | More modes may become an expensive metal-drum synth without adding rhythmic control | High timbral overlap; investigate retrigger envelope policy rather than another object bank |
| Dust | Seeded filtered noise with short amplitude windows and a bounded spectral-shape control | Unfiltered white bursts become familiar hats; extreme modulation creates aliasing or clicks | Existing noise algorithms provide baseline; exact code sharing requires matching contracts |
| Micro-fragment | Windowed readout of a bounded captured transient, with integer/sample selection and repeat interval | Dense asynchronous granular clouds hide attacks and the phrase; start with one readhead | No Impulse captured-audio equivalent; substantial overlap with Loop's memory playback |
| Imported audio | Off-thread decode, bounds validation and explicit session ownership | Browser, codec, licensing and missing-file UX are separate projects | Optional follow-up, not required to test the temporal idea |

Reuse test patterns, WAV/measurement conventions, strict decimal parsing and
JUCE accessibility/automation idioms where they match. Do not extract a universal
voice engine, ship a cross-product dependency, add oversampling, or copy Loop's
large snapshot architecture merely because it exists.

### Gaps

No source-engine perceptual comparison is completed in these notes. Alias
suppression, pressure-event headroom, granular interpolation and transition
window duration need measured and listened comparisons in the offline lab.

## What should the interface make understandable?

### Takeaway

Show the phrase first, three local rows second, and details for one selected
thing third. Always show which layer owns the audible result and where release
will return; empty time is meaningful content, not an invitation to auto-fill.

### Cited Findings

Aste requires near-black surfaces, off-white typography, restrained functional
accent, visible essential controls, exact entry, fine drag, explicit keyboard
order and accessible names. [UI system](../../docs/UI_SYSTEM.md)

Impulse's `PatternCell` is a keyboard-focusable JUCE button with title and state
description; edits call `beginChangeGesture`, `setValueNotifyingHost` and
`endChangeGesture`. Its selectors currently combine selection and audition.
[UI source](../../apps/impulse/impulse_plugin.cpp)

Loop's memory display distinguishes empty, recording, printing and playing
states, with retained generations and visible progress. The memory UI updates
at 30 Hz, while the enclosing editor also uses a separate timer. [UI source](../../apps/loop/loop_plugin.cpp)

Existing UI artifacts check child bounds and render snapshots, but are not
native accessibility or usability evidence. [Artifact helper](../../tests/editor_artifact.hpp),
[UI system limitations](../../docs/UI_SYSTEM.md)

The listening protocol allows no reliable preference and requires responses
before the answer key. [Listening protocol](../../docs/testing/LISTENING.md)

### Inferences

The two static [wireframes](../../docs/research/n01/INTERACTION.md) explore a
single-screen desktop layout at 1280×760. They deliberately retain grayscale
until an accent is musically justified; no new product color or plugin identity
is adopted. The held view separates host, phrase and fragment positions.

Rejected for this stage: eight drum lanes; hardware-style pages; waveform views
that replace the master timeline; color-only mute/hold indication; hidden
auto-filled probability; audition on ordinary selection; global keyboard
shortcuts that steal Cubase transport; a full sample browser; arbitrary nested
graphs; committing hundreds of host parameter IDs before evaluation.

The proposed usability gate is a task trial: a new user must identify a silent
span, alter one event's repetition without changing its neighbours, predict a
continue versus suspend return, and reproduce a gesture without a mouse. Log
errors, time and confidence; do not infer discoverability from a rendered image.

### Gaps

The SVGs are nonfunctional wireframes, not an editor, and do not demonstrate
accessibility APIs, input routing, automation, audio-to-UI synchronization or
Retina rendering. No native VoiceOver, contrast-tool or Cubase interaction test
has been completed. Proposed sizes and target bounds require implementation QA.
