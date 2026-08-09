# Sequence S-01 specification

Sequence is a monophonic programmed-current synthesizer. It takes the immediate
musical role of a compact bass sequencer but uses an original dual-oscillator,
dual-filter voice rather than reproducing a commercial instrument or panel.

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
two polyBLEP oscillators + sub
                           |
bounded driven mixer
                           |
12 dB state filter <-> 24 dB ladder-informed filter
                           |
ADSR amplitude contour -> controlled output saturation
```

The filter-form control is a continuous crossfade between two simultaneously
running low-pass structures. `0%` is the broad two-pole state response; `100%`
is the heavier four-pole ladder-informed response. It is not a switch and does
not claim component-level emulation.

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
