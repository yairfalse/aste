# Cycle 30 — End-to-end 73/33 prototype

## What changed

Added an explicitly lab-only Density processor mode that oversamples the
crush-path nonlinear stages with β5 73/33 half-band filters and delays the dry
path by the resulting 44 samples. The default plugin path remains unchanged.

## Why

Isolated filter benchmarks could not prove complete signal-path alignment or
the whole-instance CPU budget.

## Evidence

- Reported, dry, and wet latency all measure exactly 44 samples.
- Fixed 127-sample and thirteen-size variable-block renders are sample-identical.
- Full-chain production CPU measures 0.2309–0.2314% across three runs.
- Full-chain oversampled CPU measures 1.0417–1.0431%; every run fails the 1%
  default-quality budget.
- Incremental cost is 0.8104–0.8118 percentage points.
- Core allocation tests cover both default and oversampled process calls.
- The deterministic no-timing report SHA-256 is
  `350d00139ff2215a3944a5837268ce901f8fe7aa596c0249fca092a12816785b`.
- Release and ASan/UBSan VST3 suites both pass 19/19; sanitizers retain all
  chain checks and skip only timing.

## Risks

The prototype is not reachable from VST3, has no serialized quality parameter,
and has not been heard in musical comparisons. It fails the default CPU gate,
and dynamic latency changes would require explicit host-safe policy.

## Next step

Write the quality-mode and latency-strategy ADR before deciding whether 73/33
is Studio-only or requires further optimization before host integration.
