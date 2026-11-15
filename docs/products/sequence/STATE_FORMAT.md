# Sequence S-01 state

Sequence uses product-local XML state with an explicit schema:

```xml
<sequence-s01 schema="3" product="sequence-s01" behavior="1">
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

Schema 3 adds a required root `behavior` value: `0` retains the original MIDI
release/legato and buffer-local PPQ arithmetic; `1` uses fresh attacks during
release and a continuous sample clock anchored to host PPQ. Schemas 1 and 2
restore behavior 0, which survives subsequent schema-3 saves. New instances
use behavior 1. Invalid or missing schema-3 behavior rejects the whole state.
This is session metadata, not an added automation parameter. The editor marks
legacy sessions. Factory presets retain the session's behavior choice.

The continuous clock reanchors on tempo changes, playback restart or a PPQ
discontinuity greater than half a sample. Sub-sample reporting roundoff does
not reanchor it. No gate threshold epsilon or audio tolerance is introduced.
