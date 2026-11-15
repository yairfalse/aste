# Impulse I-01 specification

Impulse is an eight-object rhythm and transient instrument whose starting
vocabulary is tiny clicks and deep subs, separated by structural silence.
Kick, burst, crack, metallic and resonant objects remain available deliberately.
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

Track selection is silent. Explicit AUDITION plays without MIDI or host
playback; RHYTHM DETAILS reveals cycle controls. New instances start with
`Clicks / sub`: Click on steps 4/8/15 and Low on 1/11. Eleven positions remain
empty. Click Decay is 5 ms; Low is 42 Hz with 260 ms Decay, Tone/Drive zero.
`Cuts / pressure` substitutes an 8 ms Cut and a 36 Hz / 380 ms Low, with the
same event positions. Energy is 18% in both. These are existing-engine snapshots,
not new synthesis modes, and their sound still needs listening evaluation.
Both use a shared 16-step groove and predictable hits;
polymeter, probability, variation and mutation are deliberate choices rather
than the first-use default. Factory starting points reset the complete sound
and pattern state instead of inheriting settings from the previous selection.
