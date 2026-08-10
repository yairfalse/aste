# ADR 0013: Impulse visible patterns and eight objects

- Status: accepted for internal beta
- Date: 2026-08-10

## Decision

Impulse owns eight fixed synthesized objects: Kick, Click, Burst, Body, Low,
Crack, Metal, and Cut. Each owns 32 automatable tri-state pattern parameters:
Off, Hit, and Accent. The visible pattern is the sole ordinary playback source.
Pulses and Rotation remain as inputs to an explicit Generate From Pulses action.

The editor always shows all eight pattern rows. A direct track selector chooses
one sound/cycle editor below the grid; it does not hide pattern programming.

## Why

Four voices were too narrow for the intended clicks-and-cuts instrument, and
the prior Euclidean scheduler had no place to see or edit events. Adding a
separate sequencer model or opaque pattern blob would make automation and state
harder. Ordinary integer parameters give the host, state validator, UI, tests,
and audio thread one deterministic representation.

## Consequences

Schema 2 has 368 parameters and migrates schema 1 with documented defaults.
The audio thread copies fixed atomic values into stack-owned parameters once per
block and allocates or locks nothing. Generator changes are UI-thread actions.
The eight-object stress render has a provisional 2% single-core budget; actual
DAW measurements on the supported Intel and Apple Silicon machines remain open.

