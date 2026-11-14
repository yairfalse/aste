# Field F-01 state format

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
