# ADR 0007: Harmonic internal-beta topology

- Status: accepted for external musical evaluation
- Date: 2026-08-09

## Decision

Promote lab Candidate 3 into Harmonic H-01's first product-owned processor. Four
serial bands run from Foundation through Air. Each band applies the established
minimum-phase proportional-Q contour; boosted material additionally receives
the measured bounded state-variable difference. Cuts remain linear. The graph
has fixed zero algorithmic latency and no oversampling.

The internal beta exposes Input, four band Gain/Frequency pairs, Harmonic,
Output, and host Bypass. Parameter IDs and schema 1 are frozen for session
recall. Bias, compensation, quality modes, amplifier stages, analyzer UI, and
dynamic oversampling are omitted because current evidence does not define
trustworthy behavior for them.

## Evidence

Candidate 3 passed the six-rate contour, phase, harmonic, intermodulation,
folded-harmonic, impulse, step, overload, recovery, and determinism gate in
[Cycle 4](../products/harmonic/CYCLE_04.md). It objectively outperformed the
pre-emphasis candidate while preserving monotonic nonlinear participation.

## Consequences

This decision creates an internal beta, not a release candidate. Serial
multi-band interaction, automation, CPU, host compatibility, and musical
preference must be measured on the product graph. A future topology change must
migrate schema 1 without changing the existing parameter IDs.
