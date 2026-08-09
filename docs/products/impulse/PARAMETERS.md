# Impulse I-01 parameter contract

Schema 1 contains 60 stable parameters: eight global controls and thirteen for
each of four tracks.

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

Track prefixes are `kick`, `click`, `burst`, and `body`. Every prefix owns:

| Suffix | Range | Meaning |
|---|---:|---|
| `_level` | 0–100% | object level |
| `_pitch` | 25–10000 Hz | fundamental/excitation frequency |
| `_decay` | 5–3000 ms | resonant decay |
| `_tone` | 0–100% | exciter/body balance |
| `_drive` | 0–100% | bounded amplifier loading |
| `_length` | 1–32 steps | independent cycle length |
| `_pulses` | 0–32 | Euclidean event count, bounded by length |
| `_rotation` | 0–31 steps | pattern phase |
| `_probability` | 0–100% | seeded event acceptance |
| `_ratchet` | 1–4 | subdivisions per active step |
| `_timing` | -49–49% step | track microtiming |
| `_condition` | 1–4 cycles | fires every N cycles |
| `_accent` | 0–100% | first-step cycle emphasis |

All controls support host automation, numeric entry, fine drag, reset, and
portable state. Direct parameters are deliberately playable and can produce
hard event or timbre changes; Output alone is smoothed because it has no event
semantic.
