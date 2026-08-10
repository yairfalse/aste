# Field F-01 specification

Field is a playable spatial-memory instrument. It accepts mono or stereo audio
and MIDI note excitation, stores that energy in a moving feedback field, and
returns a dense stereo image. It is not presented as a room simulator, plate
clone, shimmer preset, or transparent mastering reverb.

## Musical contract

One press of **FOREVER** changes the instrument from a long decaying field to a
held state. The transition is smoothed; it does not clear memory, change
topology, or jump gain. A second press returns to the Mass-controlled decay.
While held, grain, pitch, and motion gain a small coordinated contribution so
the memory remains alive without requiring a setup page.

The remaining controls expose phenomena:

- **Mass** changes how slowly ordinary spatial energy falls away.
- **Grain** changes the amount of deterministic stepped tape-head movement.
- **Pitch** returns two windowed pitch voices, a fifth and octave, into memory.
- **Motion** changes smooth and stepped delay travel.
- **Distance** darkens both entry into the field and feedback recovery.
- **Blend** is an equal-power dry/wet balance.
- **Output** is final level trim.

The effect accepts MIDI notes on every channel. Each note creates a short tuned
electrical excitation at the event's sample offset; note-off has no special
meaning. MIDI does not change or serialize hidden parameters.

## Signal graph

```text
audio input ----> distance input filter --+-----------------------+
                                          |                       |
MIDI note -----> short sine excitation ---+                       |
                                                                  v
                 +--------------------------------------- 8-line field
                 |     Householder feedback                    |
                 |     per-line damping                        |
                 |     smooth + grain delay motion             |
                 |                                             |
                 +-- dual-head +7/+12 pitch return <------------+
                                      |
dry ------------------------------------------------ equal-power blend
                                                         |
                                                   output trim
```

The feedback matrix is orthogonal before damping. Pitch return is accompanied
by a proportional reduction in direct feedback so increasing Pitch remains
bounded. Every delay write passes a finite clamp and `tanh`; FOREVER remains
below unity retention. No noise, nondeterministic drift, convolution, IR,
lookahead, oversampling, background thread, or hidden quality reduction exists.

## Processing contract

- C++20 DSP core with no JUCE dependency.
- Mono and stereo input/output; mono receives one side of the generated field.
- Supported measurement rates: 44.1, 48, 88.2, 96, 176.4, and 192 kHz.
- Zero reported and measured algorithmic latency.
- Fixed 1.5 MiB processing-state budget; current object is below the gate.
- No processing allocation, free, lock, file access, write, logging, or throw.
- Non-finite input and parameters are sanitized; output remains finite.
- Motion and MIDI excitation are deterministic across block partitions.

The 120-second VST tail declaration is a practical host hint, not a claim that
held energy ends at 120 seconds. Hosts may stop calling effects after their own
silence policy; FOREVER therefore cannot guarantee persistence across host
suspend, plugin removal, or project close.

## Current limits

The parameter state is recalled, but live delay memory is intentionally not
serialized in schema 1. FOREVER therefore recalls as enabled while its previous
audio field begins empty after project reload. The two pitch intervals are an
internal musical design, not user-selectable intervals. True multichannel,
sidechain input, tempo quantization, freeze-to-preset, and audio-memory export
are outside this prototype.
