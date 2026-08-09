# Cycle 1 — Harmonic research boundary

## What changed

Created Harmonic H-01's first product and DSP research specification. It defines
the asymmetric boost/cut intent, four candidate algorithm families, mandatory
measurements, production-selection gates, UI hierarchy, reuse boundary, and
implementation sequence without reserving unstable parameters or creating a
plugin shell.

## Why

Harmonic's identity depends on filter/nonlinearity interaction. Building a
conventional EQ and adding saturation afterward would prematurely decide the
central research question and encourage permanent host IDs around an unproven
graph.

## Evidence

- Repository documentation validation requires the specification and checks
  every local link.
- Each candidate has a falsifiable comparison against a linear or scientific
  reference.
- Production entry requires magnitude, phase, harmonic, intermodulation,
  aliasing, automation, latency, CPU, and blind-listening evidence.

## Risks

No candidate has been rendered yet, so the four-band layout, control names,
frequency steps, parameter ranges, and hero macro remain hypotheses. This cycle
does not make Harmonic a working plugin.

## Next step

Implement the linear proportional-Q reference and the band-residual candidate
inside the existing command-line DSP lab, then compare boost/cut bandwidth,
phase, harmonics, and neutral null behavior.
