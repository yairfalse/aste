# Field F-01 parameter contract

Schema 1 has nine stable VST3 parameters. Percentage controls map linearly at
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
