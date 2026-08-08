# State format

State is a versioned, UTF-8 product-local XML document inside the VST3 host
state block:

```xml
<density-d01 schema="1" product="density-d01">
  <PARAM id="density" value="50.0"/>
</density-d01>
```

Only stable parameter IDs and portable scalar values are stored. Unknown keys
are ignored, missing keys receive documented defaults, duplicates are rejected,
and non-finite/out-of-range values are rejected or clamped before publication.
Parsing occurs off the audio thread; a complete validated snapshot is then
published at a process boundary.

The VST3 adapter serializes JUCE's portable XML representation into its host
state block. The root must be `density-d01` with integer `schema=1` and matching
`product`; malformed or mismatched state leaves the current configuration
unchanged. A round-trip test verifies deterministic bytes and equivalent values.
Schema 1's explicit migration function is still required before the first
external build.

Schema 1 contains no quality or oversampling field. The first external Density
build has one fixed 1x, zero-latency topology under ADR 0002; no speculative
identifier is reserved in presets or host state.

The full restoration boundary also receives 3,072 deterministic fuzz cases:
arbitrary bytes, truncations, bit flips, appended data, and valid binary XML
containing hostile schema, product, identifier, duplicate, missing, non-finite,
and extreme-value mutations. Every case must leave parameters bounded, emit
stable state bytes, and process identical finite audio after reset.
