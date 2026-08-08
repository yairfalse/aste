# Research log

Density research is recorded as immutable development-cycle evidence under
`docs/products/density/`. The current algorithm narrative lives in
[docs/products/density/DSP_RESEARCH.md](docs/products/density/DSP_RESEARCH.md),
historical sources in
[docs/research/schematics/catalog.yaml](docs/research/schematics/catalog.yaml),
and architectural decisions in `docs/adr/`.

Recent sequence:

- Cycles 19–32: nonlinear aliasing and oversampling candidates; production 1x
  retained by ADR 0002.
- Cycles 33–35: release ledger, CPU evidence, and reviewed UI artifacts.
- Cycles 36–42: automation measurement and smoothing corrections.
- Cycles 43–47: blind audition packs, stereo stability, Density monotonicity,
  and deterministic keyboard navigation.
- Cycles 48–58: repository policies, CI foundation, complete parameter text
  contracts, build provenance, x86_64/universal VST3 validation, standalone
  binary-host smoke testing, macOS 15 parser portability, and compiler-aware
  warning policy, public CI diagnostics, and generated-tree policy isolation.

New entries belong in a cycle report with the required What changed, Why,
Evidence, Risks, and Next step sections. Chat history is not an engineering log.
