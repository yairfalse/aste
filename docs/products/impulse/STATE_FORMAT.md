# Impulse I-01 state

Impulse stores 368 scalar parameters in product-owned XML:

```xml
<impulse-i01 schema="2" product="impulse-i01">
  <PARAM id="seed" value="1701"/>
  <PARAM id="kick_length" value="15"/>
  <PARAM id="click_probability" value="85"/>
  <PARAM id="kick_step_01" value="2"/>
</impulse-i01>
```

This includes every visible pattern cell, cycle, sound, probability, condition,
mutation, and seed needed to reproduce playback from host PPQ. No mutable
random-engine state, sample file, widget state, or compiler layout is stored.

Missing known IDs restore defaults and unknown IDs are ignored. Duplicate known
IDs, non-finite values, malformed roots, cross-product state, and unknown schema
versions leave current state unchanged. Values are bounded before publication.

Schema 1 is accepted and migrated. The original four sound/cycle tracks retain
their values and their visible patterns are reconstructed from the restored
Length, Pulses, and Rotation values, preserving old playback. The four new
objects receive documented defaults.
