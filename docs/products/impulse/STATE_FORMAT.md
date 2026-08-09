# Impulse I-01 state

Impulse stores 60 scalar parameters in product-owned XML:

```xml
<impulse-i01 schema="1" product="impulse-i01">
  <PARAM id="seed" value="1701"/>
  <PARAM id="kick_length" value="15"/>
  <PARAM id="click_probability" value="85"/>
</impulse-i01>
```

This includes every cycle, sound, probability, condition, mutation, and seed
decision needed to regenerate a pattern from host PPQ. No mutable random-engine
state, sample file, widget state, or compiler layout is stored.

Missing known IDs restore defaults and unknown IDs are ignored. Duplicate known
IDs, non-finite values, malformed roots, cross-product state, and unknown schema
versions leave current state unchanged. Values are bounded before publication.
