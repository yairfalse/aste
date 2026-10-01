# ADR 0016: Playable defaults and tape recall

Status: accepted for internal prototype, 2026-10-01.

## Context

First-use feedback found the instruments difficult to explore and Impulse too
unpredictable. Loop also discarded recorded material on project reopen.
Automated DSP success did not establish a usable musical workflow.

## Decision

Keep parameter IDs, ranges and processing topologies stable. Give new Impulse
instances a shared 16-step groove with variation/mutation off and probability
at 100%. Schema 3 distinguishes these defaults from schemas 1 and 2, whose
missing values retain historical defaults. Existing stored patterns and sound
settings are not rewritten. Factory starting points now reset the complete
parameter set; experimental patterns remain explicitly labelled choices.

Expose drum audition, musical note names, control explanations, readable level
feedback, selected preset names with an edit marker, and explicit first-action
and mode cues. Correct fixed meter layouts at smaller supported sizes. Field
distinguishes an armed empty hold from held audio and offers Clear Field; its
hold-induced grain, pitch and motion remain unchanged and are explained.

Loop schema 3 includes three stereo tapes, their generation IDs, active deck,
and source sample rate. Samples are finite, bounded, little-endian float32 in
standard Base64 inside the existing XML envelope. Lengths and payload sizes
are validated before publication. Schemas 1 and 2 still restore controls and
empty tape memory. Recorded duration is preserved by linear resampling when
the session sample rate changes.

The audio thread publishes immutable tape snapshots using preallocated
1024-frame stereo copy-on-write pages and three pinned snapshot slots. Save
copies the last published processing range without waiting on the audio
thread. Restore builds a new engine off-thread, swaps it at a callback boundary,
and retires the old engine for non-audio-thread deletion. Lifecycle mutexes
are only used by prepare/save/restore, never processing.

## Consequences

Recall restarts tape playback from its beginning; it is not a serialization of
every DSP integrator. A partial capture is saved, but an unfinished RELOOP
destination is not yet a retained generation and is excluded. Field's live
reverb memory is still deliberately not serialized.

Snapshot capacity costs approximately five times the raw tape capacity,
including the existing playback buffers: roughly 88 MiB at 48 kHz for three
16-second stereo decks, scaling with sample rate. Restore temporarily needs
another engine plus decoded state. This is a bounded, explicit memory tradeoff
for uninterrupted recording and race-free save; it is not the Field memory
budget. The core lab can omit snapshots; `loop_lab --benchmark-snapshots`
measures the plugin's enabled snapshot path separately.

No listening-quality or real-DAW compatibility claim follows from these fixes.
The revised controls and defaults still require user audition in real projects.
