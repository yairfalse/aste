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
