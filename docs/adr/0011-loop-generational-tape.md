# ADR 0011 — Loop generational tape memory

Status: accepted for internal prototype, 2026-08-10. Supersedes the single
memory topology in ADR 0009; its pitch and state-snapshot boundaries remain.

## Decision

Loop owns three preallocated 16-second stereo tape decks. Initial capture writes
the active deck. A RELOOP action records the currently transformed playback
through a bounded record/reproduce model into another deck in real time. On
completion that print becomes the active generation. The current and two most
recent retained generations are navigable; a later print reuses the oldest deck
without allocating.

The print stage applies record loading, tape-speed-dependent bandwidth,
resolution loss, and bounded level loss. Pitch, varispeed, reverse, splice, and
transport modulation happen before the print point, so repeated RELOOP actions
make those transformations cumulative. This is a behavioral generational model,
not an exact emulation or a cosmetic lo-fi layer.

RELOOP and generation navigation are discrete lock-free requests rather than
continuous host parameters. MIDI performance commands retain sample-offset
timing. Schema 2 adds the scalar Tape Speed parameter and migrates schema 1 with
the default calibration. Captured audio and generations remain outside state.

## Consequences

- Unlimited generation numbers use bounded memory; only three generations are
  available for immediate navigation.
- A print takes one loop duration and cannot be retriggered while already in
  progress.
- Maximum free duration changes from 30 to 16 seconds to keep three-deck memory
  bounded at 192 kHz.
- Audio-thread allocation, locking, filesystem access, worker threads, and
  latency remain absent.
- Session recall still needs a race-free audio snapshot protocol before Loop can
  become a release candidate.
