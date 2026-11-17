# Sequence S-01 parameters

Schema 4 exposes 90 stable scalar parameters: 26 voice/transport controls and
four controls for each of 16 steps. Schema-1 IDs retain their original order;
the schema-2 controls and all step IDs retain their indices. `voice_mode` is
appended at host index 85. Original mono retains the old synthesis; Poly and
Mono use the new voice. All modes use mono while the host pattern runs.

| ID | Range | Default | Smoothing / meaning |
|---|---:|---:|---|
| `pressure` | 0–100% | 18% | 10 ms coordinated macro |
| `shape` | saw–pulse–sine, 0–100% | 8% | 10 ms waveform morph |
| `pulse_width` | 10–90% | 50% | 10 ms pulse duty cycle |
| `osc_mix` | 0–100% | 50% | 10 ms oscillator balance |
| `detune` | -12–12 st | 0.14 st | frequency ratio per sample |
| `sub` | 0–100% | 8% | 10 ms sub level |
| `cutoff` | 30–18000 Hz | 1600 Hz | skewed, 15 ms |
| `resonance` | 0–100% | 18% | 15 ms |
| `filter_form` | open–weight, 0–100% | 15% | 20 ms shared-state pole weighting |
| `filter_drive` | 0–100% | 25% | 10 ms nonlinear input loading |
| `env_amount` | 0–100% | 35% | filter contour depth |
| `attack` | 0.2–2000 ms | 18 ms | envelope coefficient |
| `decay` | 5–4000 ms | 650 ms | envelope coefficient |
| `sustain` | 0–100% | 78% | envelope level |
| `release` | 5–5000 ms | 550 ms | envelope coefficient |
| `glide` | 0–1000 ms | 70 ms | active for legato/slide changes |
| `output` | -24–6 dB | -8 dB | 5 ms |
| `root` | MIDI 24–60 | 36 | pattern base note |
| `division` | 1/8, 1/16, 1/32 | 1/16 | host-clock division |
| `sequence` | off/on | off | deterministic transport mode |
| `bypass` | off/on | off | instrument silence |
| `voice_mode` | Original mono / Poly / Mono | Poly | mode changes clear voices |

Each step owns `step_NN_note` (-12–12 semitones), `step_NN_gate`,
`step_NN_accent`, and `step_NN_slide`. Step identifiers are zero-padded and are
release contracts. Slide holds the marked step's gate and glides into the next
enabled step. The editor provides direct drag, typed value, reset, and one-click
flag editing without pages.

The appended `master` is a final -36..+18 dB level stage (fresh +3 dB; missing
from an older state = 0 dB). The new `filter_attack` (0..2000 ms, default 0),
`filter_decay` (0..4000 ms, default 0), and `filter_sustain` (0..100%, default 12%)
control the new voice's filter contour independently. Zero attack/decay follows
the corresponding amplifier control. Original mono ignores these three.
Host indices 0–85 retain the 0.4.0 contract; Master is 86 and filter controls
are 87–89. Shift-drag offers tenfold finer movement; exact text entry remains.

## Original mono Pressure mapping

For normalized `p`, the production mapping is monotonic:

- mixer drive: `1 + 4.5p`;
- envelope depth multiplier: `1 + 0.65p`;
- accent gain: `0.12 + 0.38p`;
- ladder loading: `p`.

The output stage constrains the coordinated level increase. No random drift or
noise is attached to the macro.

Fresh plugin defaults match Warm Brass. The core `Parameters{}` retains
the original defaults for standalone legacy callers; the plugin supplies the
new explicit defaults. Double-click resets controls to fresh plugin values.
