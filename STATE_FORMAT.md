# State format

State is a versioned, UTF-8 product-local XML document inside each VST3 host
state block. Product roots and schemas are deliberately independent:

```xml
<density-d01 schema="1" product="density-d01">
  <PARAM id="density" value="50.0"/>
</density-d01>

<harmonic-h01 schema="1" product="harmonic-h01">
  <PARAM id="harmonic" value="35.0"/>
</harmonic-h01>
```

Only stable parameter IDs and portable scalar values are stored. Unknown keys
are ignored, missing keys receive documented defaults, duplicates are rejected,
and non-finite/out-of-range values are rejected or clamped before publication.
Parsing occurs off the audio thread; a complete validated snapshot is then
published at a process boundary.

The VST3 adapters serialize JUCE's portable XML representation into host state.
The root, integer schema, and product must match the receiving product;
malformed or cross-product state leaves the current configuration unchanged. A
round-trip test verifies deterministic bytes and equivalent values. Every
restored tree passes through its product migration boundary before parameter
validation. Schema 1 is the current identity migration; unknown older or newer
schemas are rejected without changing active state.

Schema 1 contains no quality or oversampling field. The first external Density
build has one fixed 1x, zero-latency topology under ADR 0002; no speculative
identifier is reserved in presets or host state.

Density's mature restoration boundary receives 3,072 deterministic fuzz cases:
arbitrary bytes, truncations, bit flips, appended data, and valid binary XML
containing hostile schema, product, identifier, duplicate, missing, non-finite,
and extreme-value mutations. Every case must leave parameters bounded, emit
stable state bytes, and process identical finite audio after reset.

Harmonic's first external boundary covers arbitrary bytes plus structured root,
schema, product, identifier, duplicate, missing, non-finite, and extreme-value
mutations. Its exact schema contract is in
[docs/products/harmonic/STATE_FORMAT.md](docs/products/harmonic/STATE_FORMAT.md).
