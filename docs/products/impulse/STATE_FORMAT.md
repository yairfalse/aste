# Impulse I-01 state

Impulse stores 368 scalar parameters in product-owned XML:

```xml
<impulse-i01 schema="5" product="impulse-i01" behavior="1" clock="1">
  <PARAM id="seed" value="1701"/>
  <PARAM id="kick_length" value="16"/>
  <PARAM id="click_probability" value="100"/>
  <PARAM id="kick_step_01" value="2"/>
</impulse-i01>
```

This includes every visible pattern cell, cycle, sound, probability, condition,
mutation, and seed needed to reproduce playback from host PPQ. No mutable
random-engine state, sample file, widget state, or compiler layout is stored.

Missing known IDs restore defaults for their schema and unknown IDs are ignored. Duplicate known
IDs, non-finite values, malformed roots, cross-product state, and unknown schema
versions leave current state unchanged. Values are bounded before publication.

Schema 5 additionally requires `clock="0"` (historical) or `clock="1"`
(sample-owned). Schemas 1–4 restore clock 0 without changing their timing.
New instances use clock 1; explicit current-response selection upgrades a
recalled instance. Invalid/missing clock rejects the entire schema-5 restore.
Clock 1 is used only with corrected sound behavior. Its sample timeline anchors
to host PPQ and re-anchors on start, tempo change, seek or a discrepancy over
half a sample. It preserves sounding tails and reevaluates the destination step
on discontinuity; negative PPQ does not trigger steps. No timing epsilon is added
to step boundaries. Both choices survive save/reopen and are not host parameters.

Schemas 4–5 require behavior 0 (legacy) or 1 (corrected). Schemas 1–3 restore 0;
re-saving preserves that choice. Corrected MIDI variation uses a per-track
event ordinal, reset on prepare/reset or a playing transport discontinuity,
not the callback-local sample offset. Repeated stopped MIDI performances are
repeatable from reset, not independently reseeded on every note. Low Tone
controls its existing harmonic edge from 0.03 to 0.21; legacy retains 0.12.
No new voice, host parameter or modulation system is added.

New instances select appended `Clicks / sub` (index 6): three Click events and
two Low events. `Cuts / pressure` (index 7) substitutes a Cut and a deeper Low
on the same rhythm. Both are complete parameter snapshots with behavior 1.
The scene values are unchanged by schema 5; timing choice belongs to the session.
All six preceding snapshot indices/names/values are retained, including
`Space / two objects` (index 5): three hits, two tracks, no random variation,
reduced Energy and shorter Kick decay.
The five prior preset indices, names and values remain intact and select
legacy sound behavior. Old partial-state defaults remain those of their
schema, not the new starting point. Space and both click/sub scenes select
corrected behavior. Older schema-4 builds can recall these new scenes because
state stores parameters, not the preset menu index.
Track selection is silent; explicit AUDITION plays the selected object.
RHYTHM DETAILS reveals cycle/generation controls without changing their values.
Legacy Low Tone is visibly disabled rather than pretending to shape audio.

Schema 1 is accepted and migrated. The original four sound/cycle tracks retain
their values and their visible patterns are reconstructed from the restored
Length, Pulses, and Rotation values, preserving old playback. The four new
objects receive documented defaults.

Schema 2 retains the historical polymetric defaults, including missing values.
Schema 3 defaults to the shared 16-step Steady Start pattern, 100% probabilities,
and zero variation/mutation. Stored parameter values always take precedence;
loading old projects does not apply the new starter groove.
