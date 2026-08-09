# ADR 0009 — Loop memory, clock, and pitch boundary

Status: accepted for internal prototype, 2026-08-09.

## Decision

Loop L-01 owns a preallocated 30-second stereo circular buffer. Capture and
playback run in the product DSP with no audio-thread allocation, lock, file
access, worker thread, or product-to-product dependency. Host-sync length is a
stable beat parameter converted from current BPM; free length has a separate
seconds parameter so automation never changes a parameter's unit.

Varispeed advances the loop read position and therefore changes pitch and
duration together. Pitch uses a deliberately constrained overlapping dual-head
window while the base read position continues at the selected varispeed. It is
characterful and duration-preserving, not a transparent phase vocoder.

Schema 1 stores control state but not captured samples. This is an explicit
prototype limitation: copying a live 30-second buffer during host state capture
without blocking or racing the audio thread needs a product-owned snapshot
protocol. No unsafe lock or speculative shared storage service is introduced to
hide that missing work.

## Consequences

- Control and preset recall is deterministic; captured audio is cleared when a
  new instance is prepared.
- MIDI note-on/note-off controls capture at event sample offsets.
- The UI clear button publishes one lock-free boolean request; the audio thread
  discards its memory indexes at the next block boundary without walking or
  reallocating the 30-second buffer.
- There is no algorithmic latency and no host latency transition.
- A release candidate requires bounded, race-free captured-memory snapshots and
  migration tests before session-recall claims include audio content.
