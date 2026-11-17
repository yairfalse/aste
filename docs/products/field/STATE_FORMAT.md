# Field F-01 state format

## 0.12.0 engine revision

The schema remains 2. Optional root `engine="1"` selects the original DSP;
`engine="2"` selects the new diffused space. Absence selects 1. Schema 1 always
restores the old engine. Any unknown/non-integer revision rejects the entire
state before parameters or metadata change. `behavior` and `material` retain
their existing meanings and cannot identify the new reverb.

New instances write engine 2; recalled old sessions keep engine 1. Explicit
adoption preserves all 19 parameter values. Original nine snapshots select the
old engine; six appended starting points select engine 2. New state and preset
recall are byte-stable. The old stereo DSP path has sample-exact regression
comparison with the frozen pre-update source. Live delay/capture audio remains
unsaved, as before, so reopen begins with empty memory. Releasing and preparing
for a sample-rate change also clears live audio. Host stop/seek does not
implicitly erase a reverb tail.

## Historical behavior metadata

Current schema 2 adds required root `behavior="0"` (legacy coupled hold) or
`behavior="1"` (independent hold). Schema 1 restores behavior 0; it survives
save/reopen. Invalid/missing behavior rejects the complete schema-2 state.
New `Still Field` instances use behavior 1: FOREVER changes retention and
injection but no longer silently adds 14 percentage points to Grain, Pitch
and Motion. Damping, bounded feedback, pitch safety trim and smoothing remain.
This is still a transforming, lossy network, not a lossless freeze.

The original five presets keep their values and select behavior 0. The new
starting point is appended; parameter IDs, ranges and reset defaults remain
unchanged. Live energy is not saved in either schema. The UI now calls its
mono-tap proxy FIELD ACTIVITY and its pre-loss coefficient RETENTION TARGET;
neither is represented as total stored buffer energy or measured decay time.

Field schema 1 is a JUCE-compatible portable XML value tree with root and
product identifier `field-f01`, integer `schema="1"`, and one scalar child per
stable parameter. It contains no compiler layouts, pointers, delay buffers,
random state, or UI geometry.

Restoration validates the root, product, schema, identifier uniqueness,
presence of scalar values, finite decimal syntax, and parameter ranges before
replacing active state. Unknown identifiers are ignored for forward
compatibility; missing known values receive defaults. Duplicate or malformed
known values reject the whole document without changing current controls.

Live reverb memory is not part of schema 1. Session recall restores FOREVER and
all sound controls deterministically, then starts with an empty spatial field.
Adding portable memory recall would require a new schema, bounded storage,
cross-rate behavior, and a non-audio-thread snapshot design.

Version 0.5.0 appends `master` and `texture` after the original nine parameters
without changing schema 2. Missing Master restores unity, and missing Texture
restores zero, preserving the old field. Texture and final gain are serialized
as ordinary scalar controls. Held audio is still not saved.
