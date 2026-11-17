# Non-Machine N-01 — first playable VST3

0.7.0 development build. A separate product fork of the temporal laboratory;
research sources, fixtures and listening evidence remain independent.

## Play in Cubase

Add an **Instrument Track**, select **Non-Machine N-01**, enable DAW SEQUENCE,
and press Cubase Play. BPM and PPQ come from the host; no MIDI Clock routing is
needed. Sparse time starts with Pressure, Impact and Dust on independent
4/3/5-quarter-note cycles. The eight-quarter-note phrase reserves its last
18.75% as structural silence. Accelerating cuts and Suspended dust are explicit
starting sounds, not automatic fill generators.

HIT auditions without starting the transport. MIDI C/C#/D pitch classes excite
Pressure/Impact/Dust respectively on any channel. Selecting an event is silent.
Click one of eight event slots in a row to edit its locks, repeats, timing,
condition, articulation and sound below. Cycle Position is a percentage of that
voice's cycle; changing cycle length scales event positions. Shift-drag for fine
adjustment, type exact values, double-click to restore a control's default.

## Capture and time

LOCAL HOLD captures that voice's current audio, up to 250 ms. HOLD MASTER
captures up to 500 ms from the stereo mix. Feed sound first; the footer reports
when a requested selection has insufficient history. Length plus lookback must
fit 500 ms. The two capture banks are allocated in prepare, never in processing.

Continue advances the underlying phrase and local voices beneath master
playback. Suspend freezes that phrase/source/local state while host and master
fragment playback continue. Release resumes or reveals the underlying phrase.
REJOIN HOST explicitly returns to host phase and clears history. REVERSE MASTER
can be recorded as automation; shorter held fragment lengths apply immediately.
Release/rehold to expand a selection. HARD CUT chooses destructive boundaries;
otherwise the engine uses bounded transition tapers. VOID is the final gate and
cannot be filled by captured repeats. Stop/restart/seek clears temporal history.

Repeat trains are limited locally to one cycle of elapsed repetition time; an
excessive interval/count never blocks other pattern edits. The footer reports
limiting and active captures. Per-event sound changes apply at the next trigger.

NEW RHYTHM creates a deterministic sparse layout from the next seed, preserving
each voice's first-event sound. It releases captures so the edits are audible.
The three starting scenes have distinct positions, enabled events and cycles.
Event buttons show cycle percentages; Shift-click toggles an event.

## Parameters and state

Schema 1, product/root `non-machine-n01`; bundle `invalid.aste.non-machine-n01`,
plugin code `Non1`, Instrument/Drum. There are 445 parameters: eleven globals,
four controls per voice, sixteen controls per event × 24 events, final Master
Output, host Bypass, fifteen object-material controls and twenty-one appended
hit-sound locks (events 2–8 on each object). IDs use `v1_cycle`, `v1_e1_pitch`, etc.; controls retain
ordinary host automation and project recall. Master is -36..+18 dB after bounded
instrument loading. Future incompatible edits need explicit schema migration.

The state validator rejects malformed/cross-product/unknown-schema data,
duplicate known IDs and non-finite values. Missing controls restore defaults.
Captured audio, current source envelopes, phrase offsets and running fragment
history are **not** serialized. Saved holds remain armed and wait for new audio;
no retained recording is implied. Research API types are not the plugin state.

## Engine and limits

The independent C++20 engine is in apps/non-machine. The JUCE adapter uses
cached atomics and bounded MIDI/command arrays. DSP storage is prepared before
playback; processing never resizes capture memory or invokes file/lock APIs.
MIDI SysEx is ignored without constructing allocating messages. A one-voice
source per object and one outgoing articulation tail are deliberate bounds.
Crossfade is a 2 ms candidate; overlapping faster repeats may replace that tail.
No imported audio, song chain, sample browser or finished-product claim exists.

Supported local checks span 44.1–192 kHz, mono/stereo and irregular buffers.
Native editor minimum is 1080×780, default 1320×860; all event lanes stay visible.
Universal arm64/x86_64 VST3 is built; no N-01 AU or packaged-release component
was introduced. The six-product .pkg path remains separate. Musical acceptance
and live Cubase interaction still require the musician's next listen.

## 0.7.0 object composition

Fresh instances use Objects / build. EMPTY PATTERN disables all events while
keeping sound shapes and cycles. Click an empty event lane to add a hit, drag a
marker to place it, double-click to remove. Numbered event buttons still select
the exact inspector; timeline arrow keys move 6.25% (Shift: 0.1%) and Delete
disables the selected hit. Selection itself remains silent; HIT auditions.

The five material controls below the selected lane apply to its entire object:
Original / Pressure / Resonant metal / Noise object / Broken fragment; Structure;
Strike; Pitch Fall (-24..24 semitones); Roughness. They are read at each trigger.
Per-event pitch, decay, color, level and timing locks remain separate. Original
preserves the previous fixed source for old sessions. New material controls append
after the existing Master and Bypass host slots; missing IDs restore Original.
Material/Structure/Strike/Roughness ranges are 0..4 / 0..100 / 0..100 / 0..100.
This is a small source palette, not an artist emulation or a finished drum library.

## Material response (0.8.0)

New instances have one base sound per object. Unlocked hits follow its Level, Pitch, Decay and Color; timing and articulation remain per-hit. LOCK HIT SOUND copies the base into the selected hit before permitting an independent sound. HIT auditions the object; HEAR SELECTED HIT auditions its lock. Audition temporarily crossfades away from captures while the underlying phrase continues. Pattern mode ignores MIDI notes and reports why; disabling Pattern enables MIDI object input.

States without the root `material` attribute use the previous response. USE UPDATED SOUND preserves parameter values but changes sound ownership and body behavior. Save a separate project version before adopting when exact mix recall matters. New sound-lock automation must set its own raw sound values; initialization from the base is a UI edit action.
