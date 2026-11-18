# Loop L-01 parameter contract

Schema 2 contains 20 stable parameters. Percent parameters are exposed as
0–100 in the host and converted to 0–1 only at the DSP boundary.

| ID | Name | Range | Default | Role |
|---|---|---:|---:|---|
| `capture` | Capture | off/on | off | Latches recording/overdub |
| `overdub` | Overdub | 0–100% | 50% | New-versus-existing memory balance |
| `feedback` | Feedback | 0–100% | 85% | Existing-memory retention |
| `sync` | Host Sync | off/on | on | Selects beat or free duration |
| `length_beats` | Length Beats | 0.25–16 beats | 4 | Host-synced loop duration |
| `free_length` | Free Length | 0.05–16 s | 2 s | Unsynchronised duration |
| `start` | Start | 0–100% | 0% | Read origin within memory |
| `speed` | Speed | 0.125–4× | 1× | Varispeed rate and duration |
| `reverse` | Reverse | off/on | off | Reverses memory travel |
| `pitch` | Pitch | -12–12 st | 0 | Dual-head pitch offset |
| `splice` | Splice | 0–25% | 3% | End/start crossfade |
| `wow` | Wow | 0–100% | 8% | Slow periodic transport movement |
| `flutter` | Flutter | 0–100% | 3% | Faster periodic movement |
| `drift` | Drift | 0–100% | 2% | Deterministic slow movement |
| `degradation` | Loss | 0–100% | 8% | Resolution, bandwidth, and level lost on each print |
| `amplifier` | Record | 0–100% | 25% | Record-amplifier loading on each print |
| `tape_speed` | Tape Speed | 3 3/4, 7 1/2, 15 IPS | 7 1/2 IPS | Print bandwidth calibration |
| `mix` | Mix | 0–100% | 100% | Live/memory balance |
| `output` | Output | -24–12 dB | -3 dB | Final trim |
| `bypass` | Bypass | off/on | off | Host-owned transparent bypass |

All continuous values support host-standard fine drag, exact entry, reset, and
automation. Mix, output, speed, amplifier loading, and degradation use 5 ms
sample-domain smoothing. Loop origin, duration, pitch, and splice are direct
play controls and may create intentional edits when moved. Processing clamps
non-finite and out-of-range values. Boolean actions are immediate; no quality
or latency topology is hidden. RELOOP, Previous, Next, and Clear are
non-parameter performance actions delivered to the audio thread through
lock-free bounded requests; they do not pretend to be continuously automatable
values.

## Final master level (0.5.0)

`master` is appended after the existing host parameters, with -36..+18 dB
range and 20 ms smoothing. It acts after sound shaping and the existing Output
control. Output meters report this final level. Its header control is always
visible, with exact text entry and Shift-drag fine changes. Old sessions without
this parameter restore 0 dB. Fresh instances start at 0 dB.
The original parameter indices, ranges and existing Output behavior remain.

## Tape transport

`transport` is appended after `master` (host index 21): Play=0, Stop=1,
Release=2; default Play. Existing sessions missing it retain their old playback
behavior. The three always-visible buttons operate this same automatable choice.

Stop freezes tape position, modulation, recording and an unfinished print. A
5 ms smoothing time fades the wet signal; the dry side of Mix is unaffected.
Play resumes at that position and fades the retained tape back in. Capture's
latch is preserved but cannot write while stopped or releasing.

Release prevents new capture/printing, abandons an unfinished print, and lets
the existing playback decay without modifying retained audio. Feedback sets
the -60 dB decay time: `0.05 + 7.95 * (Feedback / 100)^2` seconds (50 ms–8 s).
Even 100% Feedback releases. Play can recover the original tape afterwards.
Clear remains the separate destructive action. Transport is independent of the
DAW transport; saving recalls the selected mode, with the existing tape-recall
rule that playback starts at the beginning rather than restoring a live head.

## Safe Takes performance workflow

Five parameters are appended at indices 22–26. The original 22 identities,
indices, ranges and missing-key defaults are unchanged.

| ID | Range / default | Meaning |
|---|---|---|
| `safe_takes` | off/on; missing-key default off | First take / new instances enable it. One-shot capture, protected history, audible print preview and region performance. Off retains the original overdub/recycling graph. |
| `region_end` | 0–100%; 100 | Right boundary; `start` is the left boundary in Safe Takes. An inverted region cannot be printed. |
| `repeat` | off/on; off | Repeat selected region while the underlying selected-loop timeline continues. Automatable latch; Hold provides a momentary gesture. |
| `repeat_division` | FREE, 1/4, 1/8, 1/16, 1/32; FREE | Fragment duration at host tempo, capped by region length. Varispeed also changes its audible duration. |
| `monitor` | off/on; off | Adds live input during retained-tape playback; excluded from RELOOP. Capture always monitors input. |

Safe capture is edge-triggered and ends at its latched duration or when Capture
is released. It ignores Overdub and Feedback when writing the new take. The
editor acknowledges automatic completion by turning Capture off; automation
without an editor must send off before the next on. It does not require a UI
timer to stop recording. Capture duration uses the tempo at the initial edge;
tempo changes and DAW seeks never resize or overwrite an existing take.
Host Sync selects beat-derived duration, **not a quantized start or PPQ-locked
playback**. Tape transport remains explicitly independent of DAW transport.

Safe RELOOP prints varispeed, reverse, pitch, drift/wow/flutter, region/splice,
repeat and the auditioned Record/Loss/IPS processing once. Monitoring, Mix,
Output, Master and the Stop/Release envelope are not printed. Length is the
selected fragment's source duration divided by Speed, capped at 16 seconds;
it is latched on entry. Parameter changes during printing are captured.
At zero Record and Loss the new coloration path is transparent. The original
graph remains available with Safe Takes off.

The source keeps playing after printing. Click the new generation's tile to
listen: this selects its actual PCM and neutralizes temporary transforms
(including splice), preserving Mix, Output, Master, capture length and
release time. This action is undoable. Older/Newer buttons do the same in Safe
Takes; legacy MIDI navigation retains its immediate transform-in-place behavior.

Hold to Repeat temporarily permits changes to start/end, speed, reverse and
pitch. Mouse/key release or focus loss restores those five controls and the
prior repeat latch. Latch keeps edits until explicitly changed. Start/end,
reverse and repeat transitions use a 5 ms fade. Splice zero allows deliberately
hard fragment seams; positive Splice crossfades them. Speed remains positive
(0.125–4x); Reverse owns direction and Stop owns zero motion.
