# Cycle 39 — Attack smoothing comparison

## What changed

Added a lab-only full-graph comparison of current block-constant Attack
automation against 5 ms and 10 ms logarithmic one-poles and a 3+3 ms
logarithmic cascade. Production Attack behavior is unchanged.

## Why

Attack was the worst isolated automation case. Smoothing in log-time space
preserves the control's logarithmic taper and exact steady-state time while
avoiding abrupt detector-coefficient changes.

## Evidence

- Current Attack curvature excess is -53.086 dBFS.
- The one-stage 5 ms candidate is best at -79.219 dBFS, a 26.133 dB reduction.
- It reaches 63.212% of the log range after 5 ms and settles within 1% in
  23.042 ms.
- The 10 ms one-pole measures -79.135 dBFS and settles in 46.063 ms.
- The 3+3 ms cascade measures -79.053 dBFS and settles in 19.917 ms.
- The unsmoothed reference matches production with exactly zero sample delta.
- Release and Address/UndefinedBehavior sanitizer matrices pass 24/24,
  including plugin lifecycle and calibrated callback safety audits.
- Production golden renders and six-rate variable-block consistency pass
  unchanged with zero measured latency and zero block-schedule delta.
- Five production CPU runs span 0.235916–0.260563% of one M4 Pro core; median
  is 0.237488%, inside the established 1% budget.
- Release and sanitizer reports are byte-identical with SHA-256
  `97ce9e4c5baefda604f36850ec8c3ed00af0ee2d2be13dbf284e7469599f2745`.

## Risks

The lab implementation adds two exponential evaluations per sample if enabled
and has not been optimized or auditioned as production behavior. The
provisional overall automation gate remains open until integration is
verified; Blend and simultaneous automation also remain above -60 dBFS.

## Next step

Make the one-stage 5 ms log-time smoother the production Attack trajectory,
then rerun automation, golden, rate/block, CPU, and real-time safety evidence.
