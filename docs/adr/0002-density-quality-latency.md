# ADR 0002: Fixed zero-latency quality for Density D-01

- Status: accepted for the first external build
- Date: 2026-08-07

## Context

The production 1x graph reports zero latency and measures 0.231% of one M4 Pro
core. A lab-only β5 73/33 oversampling prototype phase-aligns both branches at
44 samples but measures 1.042–1.043% for the complete graph, consistently
missing the 1% default-quality budget. It has not completed musical listening
selection or host-facing latency tests.

Quality changes could alter DSP topology, latency, CPU, and output samples.
Treating such a change as ordinary automation would require concurrent paths,
bounded crossfades, state transfer, and host latency renegotiation during
processing. Reserving 44 samples in every mode would instead add latency to the
current path for a mode that is not qualified.

## Decision

Density D-01's first external build has one fixed processing quality: the
current 1x graph with zero reported latency.

- Do not expose a `quality` parameter, menu setting, preset field, or automation
  ID.
- Real-time and offline rendering use identical DSP.
- Report the active topology's actual latency; do not reserve or hide latency
  for an unavailable mode.
- Keep the 73/33 mode lab-only and unreachable from the VST3 adapter.
- Do not switch oversampling topology while processing.
- Make no anti-aliasing or oversampled-processing product claim.

A future quality mode requires a successor ADR. It must define a stable state
schema migration, non-automatable transition semantics, message-thread host
latency notification, safe processor reset or crossfade behavior, and tests in
every supported host. It must also pass its declared whole-instance CPU budget
on supported hardware and musical listening review.

## Rejected alternatives

- **73/33 as default:** rejected because the measured complete graph fails the
  default CPU gate.
- **Always reserve 44 samples:** rejected because it penalizes the qualified 1x
  path without shipping a qualified alternative.
- **Live/Studio/Render selector now:** rejected because the names, state meaning,
  transition behavior, and host latency contract are not validated.
- **Oversample only during offline bounce:** rejected because real-time and
  offline renders would no longer recall the same processing configuration.
- **Silent dynamic topology changes:** forbidden because they can cause gain,
  phase, latency, and CPU discontinuities.

## Consequences

Session recall, bypass, freeze, and real-time/offline comparison retain one
deterministic zero-latency topology. The known nonlinear aliasing remains an
honestly documented limitation. The lab prototype remains useful for research
without expanding the released parameter or state contract.
