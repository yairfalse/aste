# Impulse I-01 state

Impulse stores 368 scalar parameters in product-owned XML:

```xml
<impulse-i01 schema="4" product="impulse-i01" behavior="1">
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

Schema 4 requires behavior 0 (legacy) or 1 (corrected). Schemas 1–3 restore 0;
re-saving preserves that choice. Corrected MIDI variation uses a per-track
event ordinal, reset on prepare/reset or a playing transport discontinuity,
not the callback-local sample offset. Repeated stopped MIDI performances are
repeatable from reset, not independently reseeded on every note. Low Tone
controls its existing harmonic edge from 0.03 to 0.21; legacy retains 0.12.
No new voice, host parameter or modulation system is added.

New instances select the appended `Space / two objects` starting point: three
hits, two tracks, no random variation, reduced Energy and shorter Kick decay.
The five prior preset indices, names and values remain intact and select
legacy sound behavior. Old partial-state defaults remain those of their
schema, not the new starting point. The new preset selects corrected behavior.
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
