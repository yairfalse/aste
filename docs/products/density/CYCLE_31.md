# Cycle 31 — Quality and latency decision

## What changed

Accepted ADR 0002: Density's first external build has one fixed 1x processing
quality and zero reported latency. Removed the speculative `quality` row from
the parameter contract and documented that schema 1 stores no quality field.

## Why

The aligned 73/33 prototype is technically correct but fails the default CPU
gate. Exposing it now would add an unvalidated state, automation, transition,
and host-latency contract.

## Evidence

- The production graph measures 0.231% CPU and reports zero latency.
- The aligned prototype measures 1.042–1.043% CPU and reports 44 samples.
- The VST3 adapter exposes exactly eleven IDs; none is `quality`.
- `prepareToPlay` selects the production processor and reports its actual zero
  latency to the host.
- Real-time and offline rendering therefore use the same topology.
- Production-golden and full plugin-adapter checks pass 2/2 after the
  documentation alignment; no processing code changed in this cycle.

## Risks

Production nonlinear aliasing remains documented and unresolved. Users cannot
select the measured oversampling candidate. A later quality mode will require a
state migration and a successor ADR rather than an undocumented retrofit.

## Next step

Return to the Density musical path: render level-matched production-versus-
73/33 audition fixtures and decide whether the measured alias reduction is
audibly worth pursuing before optimizing or exposing oversampling.
