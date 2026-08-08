# Cycle 41 — Blend smoothing comparison

## What changed

Added a lab-only full-graph comparison of the production 5 ms Blend trajectory
against 10 ms and 20 ms one-poles and a 3+3 ms cascade. Production Blend
behavior is unchanged.

## Why

Blend became the largest isolated automation discontinuity after Attack was
smoothed. Its trajectory must be evaluated in the real dry/crush summing graph.

## Evidence

- Current 5 ms Blend curvature excess is -56.401 dBFS and matches production
  with exactly zero sample delta.
- The 3+3 ms cascade is best at -81.452 dBFS, a 25.051 dB reduction.
- It reaches 49.742% after 5 ms and settles within 1% in 19.917 ms, versus
  63.212% and 23.042 ms for current production.
- The 10 ms and 20 ms one-poles pass at -62.150 and -67.887 dBFS but settle in
  46.063 and 92.125 ms respectively.
- Production golden renders and six-rate variable-block consistency remain
  unchanged with zero block-schedule delta and zero measured latency.
- Release and Address/UndefinedBehavior sanitizer matrices pass 25/25,
  including plugin lifecycle and calibrated callback safety audits.
- Five production CPU runs span 0.263695–0.307242% of one M4 Pro core; median
  is 0.271841%, beneath the established 1% local budget.
- Release and sanitizer Blend reports are byte-identical with SHA-256
  `162e9968bafd784aa5d9e4860742b6e6837fcd90a5a8a9802684e7803c880fc3`.

## Risks

The cascade has not been integrated or auditioned during live wet/dry moves.
Its first-sample motion is deliberately slower, though its measured settling
time is shorter. Hardware evidence remains limited to the M4 Pro.

## Next step

Make the 3+3 ms cascade production Blend behavior, then rerun automation,
golden, rate/block, CPU, real-time safety, and both VST3 validators.
