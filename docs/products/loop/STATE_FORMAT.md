# Loop L-01 state

Loop uses a product-owned portable XML scalar schema:

```xml
<loop-l01 schema="1" product="loop-l01">
  <PARAM id="feedback" value="85.0"/>
  <PARAM id="speed" value="1.0"/>
</loop-l01>
```

Known values are finite-checked and bounded. Missing values restore defaults;
unknown IDs are ignored. Duplicate known IDs, malformed roots, non-finite
values, cross-product data, and unknown schema versions leave current state
unchanged.

Schema 1 deliberately contains no captured sample buffer. Control state recalls
deterministically, but recorded memory does not survive a new instance or
session reload. A later schema may add an explicitly versioned, bounded audio
payload only after a race-free snapshot design passes real-time and fuzz tests.
