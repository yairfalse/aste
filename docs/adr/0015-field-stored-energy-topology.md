# ADR 0015: Field stored-energy topology

Status: accepted for internal prototype, 2026-08-10.

## Context

Field must behave as a playable heavy reverb, grain instrument, and persistent
pitched memory while retaining a single obvious performance action. A bank of
named room algorithms, convolution IRs, or a clean reverb followed by separate
grain and pitch effects would expose implementation categories rather than the
requested musical phenomenon. Dynamic topology changes would also make
FOREVER automation and state harder to reason about.

## Decision

Use one fixed eight-line feedback-delay network with Householder mixing,
frequency-dependent damping, deterministic smooth and stepped delay movement,
and two dual-head pitch shifters returned inside feedback. FOREVER moves the
same network continuously toward bounded near-lossless retention while
slightly increasing the existing grain, pitch, and motion contributions.

Keep +7 and +12 semitones as internal voices for the first prototype. Expose
Mass, Grain, Pitch, Motion, Distance, Blend, and Output, but no algorithm,
room, material, interval, or quality selector. Accept MIDI notes as short tuned
excitations at host event offsets. Report zero latency. Serialize controls but
not live delay memory in schema 1.

The field uses fixed arrays below a product-specific 1.5 MiB state budget. This
is an explicit exception to the general 256 KiB non-memory-effect budget; a
long reverb requires stored samples, and fixed bounded memory is preferable to
callback allocation or a speculative shared delay service.

## Alternatives rejected

- Convolution: requires licensed or original IR content and gives FOREVER,
  grain, and pitch no common physical memory.
- Named multi-algorithm reverb: increases UI and state surface before a second
  musically justified topology exists.
- External post-reverb granular and pitch blocks: sounds assembled rather than
  making imperfection emerge from recirculation.
- Modulated all-pass clone of a known unit: conflicts with the original-
  instrument and evidence rules.
- User-selectable pitch intervals in schema 1: useful later, but it weakens the
  first instrument's one-gesture identity without musical test evidence.

## Consequences

One continuous topology makes automation, recall, CPU, and stability testable.
Grain and pitch accumulate naturally because they alter stored energy. The
fixed intervals and lack of memory recall are deliberate prototype limits. The
nonlinear feedback and pitch windows require musical, alias, and long-hold
evaluation before release claims; schema 1 cannot later add memory snapshots
without an explicit migration design.
