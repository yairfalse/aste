# State format

State is a versioned, UTF-8 product-local XML document inside each VST3 host
state block. Product roots and schemas are deliberately independent:

```xml
<density-d01 schema="2" product="density-d01" behavior="1">
  <PARAM id="density" value="50.0"/>
</density-d01>

<harmonic-h01 schema="1" product="harmonic-h01">
  <PARAM id="harmonic" value="35.0"/>
</harmonic-h01>

<sequence-s01 schema="3" product="sequence-s01" behavior="1">
  <PARAM id="pressure" value="35.0"/>
  <PARAM id="step_01_note" value="0"/>
</sequence-s01>

<field-f01 schema="2" product="field-f01" behavior="1">
  <PARAM id="forever" value="0"/>
  <PARAM id="mass" value="62.0"/>
</field-f01>
```

Except for Loop's bounded tape payloads, stable parameter IDs and portable
scalar values are stored. Unknown keys
are ignored, missing keys receive documented defaults, duplicates are rejected,
and non-finite/out-of-range values are rejected or clamped before publication.
Parsing occurs off the audio thread; a complete validated snapshot is then
published at a process boundary.

The VST3 adapters serialize JUCE's portable XML representation into host state.
The root, integer schema, and product must match the receiving product;
malformed or cross-product state leaves the current configuration unchanged. A
round-trip test verifies deterministic bytes and equivalent values. Every
restored tree passes through its product migration boundary before parameter
validation. Supported schemas are product-local; unknown schemas are rejected
without changing active state. Loop schema 4 also stores bounded tape payloads;
see its product state document rather than inferring a scalar-only contract.

Density schema 2 requires root `behavior` 0 or 1. Schema 1 restores 0, retaining
the original driven detector. Behavior 1 feeds the detector from the un-driven
source, leaving Drive free to push the compressed nonlinear path, and smooths
Attack in coefficient space. Behavior 0 retains log-time Attack smoothing. The chosen
behavior survives save/reopen. Invalid/missing schema-2 behavior rejects the
entire state. All IDs, ranges and parameter reset defaults are unchanged.
The original five presets retain their values and select legacy behavior;
new instances select appended `Grip` with behavior 1. `Grip` is a new explicit
starting point, not a reinterpretation of an old preset. Older binaries reject
schema 2; retain original project copies when changing binary revisions.

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
[docs/products/harmonic/STATE_FORMAT.md](products/harmonic/STATE_FORMAT.md).
Sequence applies the same boundary to its voice and complete 16-step program;
its exact contract is in
[docs/products/sequence/STATE_FORMAT.md](products/sequence/STATE_FORMAT.md).
Loop applies the scalar boundary to 20 controls. Schema 3 adds bounded captured
audio, generation IDs, active deck and source sample rate. Schemas 1 and 2 still
restore controls with empty tape memory; the payload and migration boundary
are in [docs/products/loop/STATE_FORMAT.md](products/loop/STATE_FORMAT.md).
Impulse stores every object, visible step, cycle, probability, mutation, and
seed value as 368 portable scalars. Schema 3 introduces predictable defaults
without changing restored schema-1/2 values. Its deterministic pattern contract is in
[docs/products/impulse/STATE_FORMAT.md](products/impulse/STATE_FORMAT.md).
Field stores nine control scalars but deliberately excludes live delay memory;
its validation and empty-memory recall contract is in
[docs/products/field/STATE_FORMAT.md](products/field/STATE_FORMAT.md).
