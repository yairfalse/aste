# Impulse I-01 parameter contract

Schema 2 contains 368 stable parameters: eight global controls, thirteen sound
and cycle controls for each of eight tracks, and 32 pattern cells for each
track. The original 60 IDs remain unchanged.

Global IDs:

| ID | Range | Default | Meaning |
|---|---:|---:|---|
| `energy` | 0–100% | 45% | excitation and output loading |
| `division` | 1/8, 1/16, 1/32 | 1/16 | step duration |
| `variation` | 0–100% | 12% | bounded event variation |
| `mutation` | 0–100% | 0% | deterministic event flips |
| `seed` | 0–65535 | 1701 | reproducible decisions |
| `output` | -24–12 dB | -6 dB | final trim, smoothed 5 ms |
| `sequence` | off/on | on | host-clocked generation |
| `bypass` | off/on | off | silent instrument bypass |

Track prefixes are `kick`, `click`, `burst`, `body`, `low`, `crack`, `metal`,
and `cut`. Every prefix owns:

| Suffix | Range | Meaning |
|---|---:|---|
| `_level` | 0–100% | object level |
| `_pitch` | 25–10000 Hz | fundamental/excitation frequency |
| `_decay` | 5–3000 ms | resonant decay |
| `_tone` | 0–100% | exciter/body balance |
| `_drive` | 0–100% | bounded amplifier loading |
| `_length` | 1–32 steps | independent cycle length |
| `_pulses` | 0–32 | event count used by explicit generator |
| `_rotation` | 0–31 steps | explicit generator rotation |
| `_probability` | 0–100% | seeded event acceptance |
| `_ratchet` | 1–4 | subdivisions per active step |
| `_timing` | -49–49% step | track microtiming |
| `_condition` | 1–4 cycles | fires every N cycles |
| `_accent` | 0–100% | level added by Accent cells |

Every prefix also owns `_step_01` through `_step_32`. Values are `0` Off, `1`
Hit, and `2` Accent. These automatable integer parameters are the actual
playback pattern and are always visible in the editor.

All controls support host automation, numeric entry, fine drag, reset, and
portable state. Direct parameters are deliberately playable and can produce
hard event or timbre changes; Output alone is smoothed because it has no event
semantic.
