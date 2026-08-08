# Cycle 42 — Production Blend cascade

## What changed

Replaced the production Blend control's single 5 ms smoother with the retained
two-stage 3+3 ms cascade. Parameter identity, range, state, latency, and static
output are unchanged.

## Why

The full parallel-graph comparison reduced boundary curvature substantially
while settling sooner than the prior response.

## Evidence

- Blend curvature excess improved from -56.401 to -81.452 dBFS, a 25.051 dB
  reduction, and the selected row matches production with zero sample delta.
- Simultaneous nine-parameter excess improved from -57.012 to -65.353 dBFS.
- All nine isolated cases and the simultaneous case now pass the unchanged
  provisional -60 dBFS curvature gate.
- All four production golden fingerprints remain unchanged.
- Six-rate fixed and variable-block renders retain zero block-schedule delta,
  zero measured latency, and their previous metric ranges.
- Five CPU runs span 0.262276–0.286155% of one M4 Pro core; median is
  0.266210%, beneath the established 1% local budget.
- Release and Address/UndefinedBehavior sanitizer matrices pass 25/25,
  including plugin lifecycle and calibrated callback safety audits.
- pluginval strictness 10 passes all 78 requested rate/block combinations;
  Steinberg extensive validation passes 537/537.
- Release and sanitizer Blend and automation reports are byte-identical with
  SHA-256 `4bfc93f5977b43737926e2bf41bb3ffb9471493de73c1b8f61b369533562ce21`
  and `1146742aba5851b5d530c5815d91b793b2173eecc02d4a5b3feb1b3eb3c567d9`.

## Risks

The -60 dBFS curvature ceiling is a deterministic engineering threshold, not
a substitute for listening. Live Blend and simultaneous automation have not
been auditioned. Hardware evidence remains limited to the M4 Pro.

## Next step

Render a deterministic automation audition pack for Drive, Attack, Blend, and
simultaneous motion, then record whether any avoidable zipper noise remains.
