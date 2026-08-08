# Cycle 36 — Production Output cascade

## What changed

Replaced the production Output control's single 5 ms exponential smoother with
the retained two-stage 3+3 ms cascade. No parameter ID, range, state field,
latency, or static-setting output changed.

## Why

The cascade measured substantially lower automation-boundary curvature while
settling sooner than the previous response.

## Evidence

- Output curvature excess improved from -41.329 to -83.271 dBFS.
- Simultaneous nine-parameter excess improved from -38.175 to -49.469 dBFS.
- The lab cascade matches the production render with exactly zero sample delta.
- All four production golden fingerprints remain unchanged.
- Six-rate fixed and variable-block renders retain zero sample delta and their
  previous gain, peak, crest, correlation, reduction, and latency ranges.
- Five CPU runs span 0.231669–0.246683% of one M4 Pro core; median is 0.233320%.
- Full Release and Address/UndefinedBehavior sanitizer matrices pass 22/22.
- The calibrated callback audit still records zero allocations, locks, file
  opens, and direct writes.
- pluginval strictness 10 passes the 78 rate/block combinations; Steinberg
  extensive validation passes 537/537.
- Release and sanitizer Output/automation CSVs are byte-identical with hashes
  `7fd0b3a0f113aa54546d867a5679f94be2c147d60794d5b592c2227882df7893`
  and `6c8032e2df23dac915cf1343ad51a7158c0c76553d1773d41c86141d511bd7d8`.

## Risks

Live manipulation has not been auditioned. The whole automation gate remains
open: Drive now dominates at -48.366 dBFS, while Attack and Blend also miss the
provisional -60 dBFS ceiling. Hardware evidence remains arm64 M4 Pro only.

## Next step

Isolate Drive with the same curvature-versus-settling protocol before changing
another production smoother.
