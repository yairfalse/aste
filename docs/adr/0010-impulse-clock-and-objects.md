# ADR 0010 — Impulse clock, seed, and object voices

Status: accepted for internal prototype, 2026-08-09.

Impulse owns four fixed rhythmic objects: Kick, Click, Burst, and Body. Each
track has an independent 1–32-step length, Euclidean pulse count, rotation,
probability, 1–4 ratchets, timing offset, cycle condition, and accent. Host PPQ
is converted directly to event ticks, so block boundaries do not accumulate
clock error. Transport discontinuities reset tick memory and derive the same
event decision from position and the stored seed.

Random decisions are pure hashes of seed, track, absolute step, and ratchet.
They require no mutable random stream and reproduce after locate, restart, or
state restore. Variation changes bounded voice details; Mutation may flip a
bounded fraction of pattern decisions. Neither is described as analog drift.

Voices are original behavioral objects, not samples or named circuit clones:
pitch-descending membrane-like Kick, differentiated Click, filtered-noise Burst,
and coupled-mode Body. Energy continuously coordinates excitation and bounded
output loading without changing topology.

The first prototype deliberately omits arbitrary per-step editing and sample
import. Its identity is fast polymetric generation plus direct MIDI triggering,
not a general drum workstation.
