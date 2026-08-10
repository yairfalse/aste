# Sequence S-01 specification

Sequence is a monophonic programmed-current synthesizer. It takes the immediate
musical role of a compact bass sequencer but uses an original dual-oscillator,
single-state character-filter voice rather than reproducing a commercial
instrument or panel.

## Product phenomenon

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
- Four product-local starting points; no filesystem preset browser.

## Explicit limits

The internal beta is monophonic, host-synchronised, and zero-latency. It has no
standalone clock, swing, polymeter, song chain, audio input, modulation matrix,
polyphony, oversampling, or hardware fidelity claim. These remain absent until
musical testing identifies a concrete need.
