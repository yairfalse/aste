# Harmonic H-01 state format

The VST3 host block contains JUCE's portable UTF-8 XML representation with a
product-local root and explicit schema:

```xml
<harmonic-h01 schema="1" product="harmonic-h01">
  <PARAM id="harmonic" value="35.0"/>
</harmonic-h01>
```

Schema 1 stores only the twelve stable parameter values. Missing parameters
receive defaults; unknown parameters are ignored; duplicate IDs, non-finite
values, malformed XML, mismatched products, and unknown schemas leave the
current state unchanged. Values are clamped to their documented ranges before
publication. Processor history, meters, UI size, and framework memory layouts
are never serialized.

New instances select appended `Charged Presence` (Input +12 dB, Presence +6 dB,
Harmonic 100%, Output −15 dB). All six previous presets and parameter reset
defaults are unchanged; partial-state defaults remain the original flat EQ.
PUSH/PULL edits existing Input and Output by equal/opposite increments, bounded
by both ranges and reported as two host gestures. It changes operating point,
not guaranteed loudness. COLOR STATUS indicates whether any positive band gain
and nonzero Harmonic enable the nonlinear path; it is not an audio meter.
