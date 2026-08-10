# Impulse I-01 specification

Impulse is an eight-object rhythm and transient instrument for deep kicks,
clicks, cuts, bursts, low impacts, cracks, metallic events, and resonant bodies.
It uses synthesis rather than factory drum samples.

```text
host PPQ + visible patterns + stored seed -> polymetric event decisions
MIDI pitch classes C through G            -> eight direct excitations

event -> exciter -> resonant body -> bounded amplifier -> protection -> output
```

Kick, Click, Burst, Body, Low, Crack, Metal, and Cut each expose Level, Pitch,
Decay, Tone, Drive, Length, Pulses, Rotation, Probability, Ratchet, Timing,
Condition, and Accent. Each also owns a visible 32-step row whose cells cycle
Off, Hit, and Accent. Lengths can differ while every event remains locked to
host PPQ. Division selects eighth, sixteenth, or thirty-second steps.

The visible row is the playback source. Pulses and Rotation feed the explicit
Generate From Pulses command; changing them never invisibly rewrites a pattern.

Energy is the hero control. Variation changes bounded excitation, pitch, and
stereo details. Mutation changes a bounded deterministic subset of event
decisions. Seed is stored, automated, and restored. The same state and host
position therefore reproduce the same generated pattern.

The single scalable panel keeps all eight pattern rows visible while one direct
track selector chooses the sound controls below. Signal orange marks hits and
the current step, off-white marks accents, and no hidden pattern page exists.
