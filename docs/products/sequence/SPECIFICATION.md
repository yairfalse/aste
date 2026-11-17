# Sequence S-01 specification

Sequence is a polyphonic keyboard and monophonic pattern synthesizer. Its
new voice takes its musical direction from the user's OB-8 reference: broad
chords, brass, strings and warm sustained tones. TB-style accents and slides
inform the mono pattern performance. This is an original implementation, not
a circuit model or a claim of matching either reference instrument.

Voice Mode offers **Poly** (eight keyboard voices), **Mono** (legato keyboard),
and **Original mono** (the previous DSP). Host playback with Sequence enabled
always uses a single centred voice. Fresh instances open on Warm Brass with
Sequence off: play MIDI chords immediately. Pulse Bass and Acid Current are
explicit sequencer starting points.

## Polyphonic voice

Each of eight preallocated voices has two band-limited waveform oscillators,
a sine sub, independent amplifier and decaying filter contours, and two
cascaded TPT state-variable low-pass sections. Filter Weight continuously
interpolates the 12/24 dB outputs. Subtle deterministic tuning and pulse-width
motion plus fixed per-voice stereo positions provide width without a chorus.
Mono output folds both sides together; mono/sequencer mode is centred.

Pressure increases input loading, contour depth and accent intensity. Velocity
shapes level and contour depth. Sustain pedal (CC64), note-on velocity zero,
all-notes-off and bounded oldest/quietest-release voice stealing are supported.
Stolen voices use a 3 ms transition. Switching voice mode or entering/leaving
the host sequence clears held voices to avoid stranded notes. MIDI remains
sample-offset accurate. Oscillator phases and motion reset deterministically.

The original signal graph and Pressure mapping below apply to Original mono,
which is automatically selected for schemas 1–3. Their synthesis remains
unchanged. Existing IDs and host parameter indices remain stable.

## Product phenomenon (Original mono)

The instrument exposes **pressure**: the sensation that oscillator level,
filter loading, contour depth, accent, and final amplifier restraint belong to
one electrical system. Clockwise Pressure continuously increases mixer drive,
filter-envelope depth, accent gain, and ladder loading without changing
topology.

## Signal graph

```text
MIDI / host PPQ -> 16-step note, gate, accent, slide program
                           |
two polyBLEP oscillators + sub / saw-pulse-sine shaping
                           |
bounded driven mixer
                           |
nonlinear four-stage filter: shared 12 dB <-> 24 dB outputs
                           |
ADSR amplitude contour -> controlled output saturation
```

Filter Weight continuously interpolates the second- and fourth-stage outputs
of one state graph. `0%` is broad and open; `100%` has the heavier four-pole
response. Resonance and loading compensation prevent a destructive midpoint
level collapse. Filter Drive controls the bounded input nonlinearity. The
control is not a switch and makes no component-level emulation claim.

## Performance model

- Sixteen directly visible steps.
- Per-step semitone offset, gate, accent, and slide.
- 1/8, 1/16, and 1/32 host divisions.
- Sample-offset MIDI note handling; MIDI transposes the pattern while transport
  runs and plays the monophonic voice directly while it is stopped or disabled.
- Host PPQ determines the current step, so project recall and restart are
  deterministic.
- Six complete product-local starting points; no filesystem preset browser.
- Warm Brass, Wide Strings, Slow Glass, Pulse Bass, Acid Current, Original Mono.

## Explicit limits

The internal beta is host-synchronised and zero-latency. It has no
standalone clock, swing, polymeter, song chain, audio input, modulation matrix,
oversampling, or hardware fidelity claim. These remain absent until
musical testing identifies a concrete need.

Musical approval remains a listening task. Automated stability and recall checks
do not establish OB-8 likeness or a finished sound.
