# Impulse I-01 specification

Impulse is a four-object rhythm and transient instrument for deep kicks,
clicks, cuts, bursts, and resonant bodies. It uses synthesis rather than factory
drum samples.

```text
host PPQ + stored seed -> independent polymetric event decisions
MIDI C/C#/D/D#         -> direct Kick/Click/Burst/Body excitation

event -> exciter -> resonant body -> bounded amplifier -> protection -> output
```

Each track exposes Level, Pitch, Decay, Tone, Drive, Length, Pulses, Rotation,
Probability, Ratchet, Timing, Condition, and Accent. Lengths can differ—15, 23,
11, and 16 steps by default—while every event remains locked to host PPQ.
Division selects eighth, sixteenth, or thirty-second steps.

Energy is the hero control. Variation changes bounded excitation, pitch, and
stereo details. Mutation changes a bounded deterministic subset of event
decisions. Seed is stored, automated, and restored. The same state and host
position therefore reproduce the same generated pattern.

The single scalable panel keeps all four track contracts visible. Signal orange
marks active values and current step numbers; no genre kits, decorative motion,
or hidden pattern page exists.
