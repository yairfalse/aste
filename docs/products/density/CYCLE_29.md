# Cycle 29 — Six-rate topology finalist

## What changed

Ran the β5 73/33 and 81/33 finalists through complete nonlinear and linear
matrices at 44.1, 48, 88.2, 96, 176.4, and 192 kHz. Each topology receives 702
alias and 462 magnitude/phase measurements.

## Why

Cycle 28 measured the relevant CPU crossover but only at the most critical
individual sample rates. One topology must survive the supported-rate matrix
before an integration prototype is justified.

## Evidence

- 73/33 measures -37.266 dBc aggregate alias p95, 87/702 points above -50 dBc,
  0.039351 dB maximum magnitude deviation, and 44 samples latency.
- 81/33 measures -38.516 dBc aggregate alias p95, 86/702 points above -50 dBc,
  0.038969 dB maximum magnitude deviation, and 48 samples latency.
- 81/33 gains only 1.250 dB p95 and one threshold point; phase and passband are
  effectively tied.
- 73/33 is retained as the sole integration candidate because it preserves the
  measured 0.958–0.961% isolated CPU range instead of 81/33's 0.986–0.998%.
- The deterministic report SHA-256 is
  `cb9b17ce24b12c9b31e030193bf8d99900fa07a38cdc7098a67832b3916d6a82`.
- Release and ASan/UBSan VST3 suites both pass 18/18; refactored Cycle 27/28
  reports remain byte-stable.

## Risks

The retained CPU figure excludes the rest of Density. Adding the current
roughly 0.208% processor baseline would exceed the 1% whole-instance budget.
No musical listening comparison has selected an oversampled path. Production
remains 1x and zero-latency.

## Next step

Build a lab-only end-to-end 73/33 crush-path integration to measure total CPU,
44-sample dry alignment, block identity, and actual full-chain latency before
changing product state or host reporting.
