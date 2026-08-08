# Cycle 40 — Production Attack smoothing

## What changed

Made the one-stage 5 ms log-time Attack smoother the production trajectory.
The visible range, default, stored value, steady-state detector time, and zero
latency are unchanged.

## Why

The Cycle 39 full-graph comparison found this was the simplest and strongest
candidate for removing abrupt detector-coefficient changes under automation.

## Evidence

- Attack curvature excess improved from -53.086 to -79.219 dBFS, a 26.133 dB
  reduction, and the selected report row matches production with zero delta.
- The trajectory reaches 63.212% of its log range after 5 ms and settles
  within 1% in 23.042 ms.
- Blend is now the worst isolated parameter at -56.401 dBFS; simultaneous
  automation measures -57.012 dBFS, so the -60 dBFS gate remains open.
- All four tracked production golden fingerprints remain unchanged.
- Six-rate fixed and variable-block renders retain zero block-schedule delta,
  zero measured latency, and their previous metric ranges.
- Five CPU runs span 0.262370–0.274137% of one M4 Pro core; median is
  0.265705%, beneath the established 1% local budget.
- Release and Address/UndefinedBehavior sanitizer matrices pass 24/24,
  including plugin lifecycle and calibrated callback safety audits.
- pluginval strictness 10 passes all 78 requested rate/block combinations;
  Steinberg extensive validation passes 537/537.
- Release and sanitizer Attack and automation reports are byte-identical with
  SHA-256 `059bb4d12ddfe302d4c3c065d5c3756bfef989206e37e21c69b3da43d5845d35`
  and `7cf549c023723e015be3037f36a6b185c846a78dcc8b1b397fe406e0d08beb72`.

## Risks

The production trajectory evaluates two exponentials per sample. Local CPU
margin is wide, but the oldest supported Apple Silicon and Intel targets remain
unmeasured. Live Attack manipulation has not been auditioned. Blend and
simultaneous automation remain above the provisional curvature ceiling.

## Next step

Compare Blend smoothing profiles through the full parallel graph, keeping the
current 5 ms response as the exact production reference.
