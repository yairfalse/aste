# Field F-01 specification

## Production 0.12.0 — diffused space

New instances use the promoted 16-line Hadamard reverb, stereo input diffusion
and early reflections. MASS maps a nominal low-frequency decay of 0.25–16 s;
DISTANCE sets absorption and readout darkness. MOTION varies the space.
TRANSFORM (stable ID `texture`) enables fragment and pitched readouts and bounded
orthogonal feedback movement without the former lossy pitch-return gain trim.
GRAIN sets fragment movement, PITCH sets the transformed wet pitch amount, and
FOCUS clarifies low-mid energy and moves the pitch interval from fifth to octave.
TRANSFORM=0 provides the conventional reverb foundation. Wet return gain is
calibrated independently of dry level; BLEND, OUTPUT and MASTER remain separate.

FOREVER closes the network's input and smoothly removes decay/damping and main
read-head modulation. Orthogonal internal movement can continue. Stored energy
is retained; releasing FOREVER admits new input and resumes the selected decay.
Feed audio before holding. Clear fades the cleared output over 5 ms, leaving the
hold switch armed. Live spatial energy is not saved to projects.

Six new starting points are Clean room, Deep hall, Dark dub, Enormous space,
Evolving field and FOREVER. The nine previous snapshots remain available and
select the previous engine. The minimum 900×680 interface displays the actual
parameter values and measured late-network activity. The existing optional
phrase/inner-fragment controls continue to work independently of spatial hold.

Existing projects without the new engine metadata render through their previous
DSP; adoption requires USE UPDATED SOUND or selecting a new factory sound.
See [state format](STATE_FORMAT.md). The historical description below applies
to the previous engine; it is preserved to document old sessions.

## Historical engine description

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
- **Output** is field level trim. **Master Output** is the final clean gain.
- **Texture** exposes overlapping grains and a direct stereo pitch layer.

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
- Fixed processing object remains below 1.5 MiB. Version 0.7.0 also preallocates
  two stereo capture banks: total state below 16 MiB through 192 kHz (32 MiB
  through the internal 384 kHz clamp). No capture-time allocation or bulk copy.
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

## 0.5.0 texture revision

New Open Field, Silver Memory, Granite Cloud and White Horizon explicitly use
Texture. The original feedback response remains available at Texture 0 and is
selected automatically when restoring states without the new parameter. The
new branch reads two overlapping Hann-windowed heads per delay line, controlled
by Grain, and mixes audible fifth/octave voices into the wet output, controlled
by Pitch. Increased pitch feedback is accompanied by reduced direct feedback.
Listening approval remains required; a finite output test does not establish
that the grain texture or pitch balance is musically successful.

## 0.7.0 captured time

The Memory inside memory starting point adds an explicit second path alongside
the field: raw stereo input plus MIDI excitation -> recording ring -> immutable
captured phrase -> selected inner fragment. CAPTURE PHRASE selects the preceding
20..4000 ms once enough history exists; release returns to the live field.
REPEAT INSIDE selects the current outer position and repeats 4..1000 ms inside
that captured phrase. Its release rejoins the continuously advancing outer cursor.
SPEED 0.25..2 and REVERSE affect playback. Shortening either selection is live;
the outer cannot expand beyond the original captured length without recapture.
MEMORY / FIELD blends between the spatial network and captured material only
while capture is active, with a smoothed return. The main Blend remains dry/wet.

A two-bank swap captures in constant time. The inner loop reads the immutable
outer memory; it does not recursively record its own output. Boundary tapers
are 0.5 ms and action transitions 2 ms. Times are milliseconds, not beat-synced.
Bypass/host suspension pauses processing; no host PPQ-return claim is made.
The new preset leaves Capture and Repeat unarmed: feed audio first, then capture
and repeat deliberately. FOREVER still controls the separate spatial feedback.
