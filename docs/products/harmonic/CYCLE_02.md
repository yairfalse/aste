# Cycle 2 — first executable topology comparison

## What changed

Added the framework-independent `harmonic_lab` executable, a double-precision
peaking reference, and the first boost-only residual-excitation candidate. Its
CSV covers all six supported sample rates, ±12 dB center behavior, phase,
half-gain bandwidth, H2/H3, Harmonic sweeps, and exact neutral null.

## Why

The first product question is not whether nonlinear EQ can produce harmonics;
it is whether harmonic participation can grow without destroying the user's
requested contour. Measuring the fundamental alongside H2/H3 makes that failure
visible.

## Evidence

- Linear center gain error is below `1e-9` dB at every rate.
- Zero gain nulls exactly.
- Residual H3 rises monotonically to about -22.85 dBc at full Harmonic.
- Equal cut stays near the numerical harmonic floor.
- Full Harmonic collapses +12 dB center gain to about +2.16 dB, rejecting the
  candidate by the product contract.
- `harmonic_reference_compare` passes under CTest and writes the complete CSV.

## Risks

The provisional Q law is not yet listening-selected. The harmonic measurement
uses one coherent 1 kHz tone and one input level; it is not an alias, IMD,
multi-frequency, or musical validation result. Rejection applies to this exact
residual formulation, not every residual architecture.

## Next step

Implement the pre-emphasis/nonlinear/de-emphasis candidate and reject it unless
it maintains fundamental contour while producing controlled boost-only
harmonics across level, frequency, and sample rate.
