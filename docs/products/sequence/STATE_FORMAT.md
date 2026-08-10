# Sequence S-01 state

Sequence uses product-local XML state with an explicit schema:

```xml
<sequence-s01 schema="2" product="sequence-s01">
  <PARAM id="pressure" value="35.0"/>
  <PARAM id="step_01_note" value="0"/>
  <PARAM id="step_01_gate" value="1"/>
</sequence-s01>
```

All 85 scalar values are stored through JUCE's portable ValueTree encoding.
Missing parameters restore documented defaults; unknown identifiers are
ignored; duplicate known IDs, non-finite values, malformed roots, cross-product
state, and unknown schemas leave the current state unchanged. Values are
bounded before publication. Pattern state contains no compiler layout, random
seed, or widget state.

Schema 1 is accepted and migrated. `pulse_width` and `filter_drive` restore to
50% and 25% when absent; all prior voice and step IDs retain their values.
