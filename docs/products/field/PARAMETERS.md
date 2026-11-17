# Field F-01 parameter contract

## 0.12.0 production mapping

There are **19** stable host parameters. No IDs, indices or ranges were changed.
New-engine MASS maps 0.25–16 s nominal decay. `texture` is labeled TRANSFORM in
the editor (its host identity remains Texture); zero disables the fragment/pitch
transformation. Grain, Pitch, Motion and Focus retain their existing slots.
FOREVER holds existing spatial energy and closes input until release.

Factory sounds change initial values, not parameter reset/default contracts.
Automation is read at block boundaries and smoothed over 35 ms; MIDI excitation
keeps sample offsets. Old sessions use the old mappings documented below until
explicit adoption. The appended loop controls and Master remain functional.

## Historical parameter mappings

Schema 2 now has eleven stable VST3 parameters. The original nine retain
their indices; Master and Texture are appended. Percentage controls map linearly at
the host boundary and are smoothed for 35 ms inside the DSP. IDs are not UI
labels and must not change without a state migration.

| ID | Name | Range | Default | Processing meaning |
|---|---|---:|---:|---|
| `forever` | Forever | off/on | off | Smoothly approaches near-lossless retention and adds bounded movement |
| `mass` | Mass | 0–100% | 62% | Maps ordinary feedback retention from 0.70 to 0.985 |
| `grain` | Grain | 0–100% | 34% | Depth of deterministic held-offset delay motion |
| `pitch` | Pitch | 0–100% | 28% | Amount of +7/+12-semitone dual-head feedback return |
| `motion` | Motion | 0–100% | 24% | Delay-motion rate, smooth depth, and grain update rate |
| `distance` | Distance | 0–100% | 45% | Coordinated input and feedback high-frequency absorption |
| `blend` | Blend | 0–100% | 48% | Equal-power dry/wet balance |
| `output` | Output | -18..+12 dB | -3 dB | Smoothed final gain |
| `bypass` | Bypass | off/on | off | Host-visible bypass parameter |

All continuous controls support drag, fine adjustment, exact text entry, reset,
automation, and host text conversion through JUCE. Invalid values use documented
defaults before clamping. The five factory starting points apply ordinary
host-notifying parameter changes and contain no private state.

`master` is a final -36..+18 dB gain stage, default 0 dB. `texture` is
0..100%, default 70% at the parameter boundary. Old states without either
restore unity Master and Texture 0. Texture exposes overlapping windowed
grains and direct stereo pitch layers with up to 2.8× wet makeup, while reducing direct feedback as the
pitch return increases. Grain and Pitch still govern their respective amounts.
Fresh instances load Moving glass (70 Mass, 65 Grain, 60 Pitch, 72 Motion,
18 Distance, 65 Blend, -3 dB Output, 85 Texture, 75 Focus). Broadcast Hall and
Still Field retain Texture 0. New presets remain starting points for listening.

`focus` appends after Texture: 0..100%, reset default 0. Older states restore
zero, preserving their existing sound. Focus removes low-frequency input and
feedback buildup, opens high-frequency damping, blends continuously cycling
grain heads, and moves fifth/octave pitch ratios in opposite directions with
Motion. Zero retains the previous processing. Focus does not alter dry audio.

0.7.0 appends `loop_hold` (off), `loop_ms` (20..4000, reset 1500 ms),
`inner_repeat` (off), `inner_ms` (4..1000, reset 90 ms), `loop_speed`
(0.25..2, reset 1), `loop_reverse` (off), `loop_mix` (0..100, reset 0%).
There are 19 host parameters, including Master. Old IDs/indices remain stable.
Fresh instances load Memory inside memory, with loop mix 90%, capture/repeat off.
Older states without new IDs restore both holds off and loop mix zero. Live
capture buffers and read cursors are not session state; restore clears history.
