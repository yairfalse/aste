# Sequence S-01 parameters

Schema 1 exposes 83 stable scalar parameters: 19 voice/transport controls and
four controls for each of 16 steps.

| ID | Range | Default | Smoothing / meaning |
|---|---:|---:|---|
| `pressure` | 0–100% | 35% | 10 ms coordinated macro |
| `shape` | saw–pulse, 0–100% | 25% | 10 ms waveform morph |
| `osc_mix` | 0–100% | 45% | 10 ms oscillator balance |
| `detune` | -12–12 st | 0.08 st | frequency ratio per sample |
| `sub` | 0–100% | 25% | 10 ms sub level |
| `cutoff` | 30–18000 Hz | 900 Hz | skewed, 15 ms |
| `resonance` | 0–100% | 35% | 15 ms |
| `filter_form` | state–ladder, 0–100% | 45% | 20 ms continuous crossfade |
| `env_amount` | 0–100% | 55% | bounded five-octave contour |
| `attack` | 0.2–2000 ms | 3 ms | envelope coefficient |
| `decay` | 5–4000 ms | 180 ms | envelope coefficient |
| `sustain` | 0–100% | 55% | envelope level |
| `release` | 5–5000 ms | 120 ms | envelope coefficient |
| `glide` | 0–1000 ms | 70 ms | active for legato/slide changes |
| `output` | -24–6 dB | -6 dB | 5 ms |
| `root` | MIDI 24–60 | 36 | pattern base note |
| `division` | 1/8, 1/16, 1/32 | 1/16 | host-clock division |
| `sequence` | off/on | on | deterministic transport mode |
| `bypass` | off/on | off | instrument silence |

Each step owns `step_NN_note` (-12–12 semitones), `step_NN_gate`,
`step_NN_accent`, and `step_NN_slide`. Step identifiers are zero-padded and are
release contracts. Slide holds the marked step's gate and glides into the next
enabled step. The editor provides direct drag, typed value, reset, and one-click
flag editing without pages.

## Pressure mapping

For normalized `p`, the production mapping is monotonic:

- mixer drive: `1 + 4.5p`;
- envelope depth multiplier: `1 + 0.65p`;
- accent gain: `0.12 + 0.38p`;
- ladder loading: `p`.

The output stage constrains the coordinated level increase. No random drift or
noise is attached to the macro.
