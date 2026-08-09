# Loop L-01 parameter contract

Schema 1 contains 19 stable parameters. Percent parameters are exposed as
0–100 in the host and converted to 0–1 only at the DSP boundary.

| ID | Name | Range | Default | Role |
|---|---|---:|---:|---|
| `capture` | Capture | off/on | off | Latches recording/overdub |
| `overdub` | Overdub | 0–100% | 50% | New-versus-existing memory balance |
| `feedback` | Feedback | 0–100% | 85% | Existing-memory retention |
| `sync` | Host Sync | off/on | on | Selects beat or free duration |
| `length_beats` | Length Beats | 0.25–16 beats | 4 | Host-synced loop duration |
| `free_length` | Free Length | 0.05–30 s | 2 s | Unsynchronised duration |
| `start` | Start | 0–100% | 0% | Read origin within memory |
| `speed` | Speed | 0.125–4× | 1× | Varispeed rate and duration |
| `reverse` | Reverse | off/on | off | Reverses memory travel |
| `pitch` | Pitch | -12–12 st | 0 | Dual-head pitch offset |
| `splice` | Splice | 0–25% | 3% | End/start crossfade |
| `wow` | Wow | 0–100% | 8% | Slow periodic transport movement |
| `flutter` | Flutter | 0–100% | 3% | Faster periodic movement |
| `drift` | Drift | 0–100% | 2% | Deterministic slow movement |
| `degradation` | Degradation | 0–100% | 8% | Memory resolution loss |
| `amplifier` | Amplifier | 0–100% | 25% | Bounded output-stage loading |
| `mix` | Mix | 0–100% | 100% | Live/memory balance |
| `output` | Output | -24–12 dB | -3 dB | Final trim |
| `bypass` | Bypass | off/on | off | Host-owned transparent bypass |

All continuous values support host-standard fine drag, exact entry, reset, and
automation. Mix, output, speed, amplifier loading, and degradation use 5 ms
sample-domain smoothing. Loop origin, duration, pitch, and splice are direct
play controls and may create intentional edits when moved. Processing clamps
non-finite and out-of-range values. Boolean actions are immediate; no quality
or latency topology is hidden.
