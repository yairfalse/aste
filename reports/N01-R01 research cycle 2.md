# N-01 / R-01 — articulation and harmonic memory, cycle 2

2026-11-15. Both assignments remain instrument research. The existing labs
already implement most requested experiments; this pass reproduced their
baselines, extended specific comparisons and generated new evidence. No new
plugin target, finished identity, stable public parameter set, installation or
musical acceptance is claimed. Earlier product work in the working tree was
preserved. Exact source hashes are retained because HEAD alone does not identify
those uncommitted comparison processors.

**Recommendation: continue small comparative composition/listening studies;
do not promote either laboratory to a product yet.** N-01's promising distinction
is editable ownership of phrase time and fragments. R-01's candidate distinction
is deliberately transforming retained energy, rather than merely accumulating
linear resonant tails. Neither distinction has human evaluation evidence yet.

## Reconnaissance and starting evidence

The pass read the product architecture, realtime rules, Impulse/Loop contracts,
Sequence/Field processors, existing laboratories, reports, interaction studies
and tests. Reusable infrastructure is offline WAV/CSV export, fixed event arrays,
sample-offset batching, deterministic fixtures, allocation/OS-call audit,
sanitizer and mutation conventions. The production processors are linked only
by comparison executables; neither research core depends on them or JUCE.

- [Architecture](../docs/ARCHITECTURE.md), [realtime rules](../docs/REALTIME_SAFETY.md).
- [Original N-01 report](Non%20Machine%20time%20study.md),
  [temporal interface study](../docs/research/n01/INTERACTION.md).
- [Original R-01 report](Resonator%20harmonic%20memory.md),
  [provisional core contract](../docs/research/r01/LAB_CONTRACT.md),
  [harmonic interface study](../docs/research/r01/INTERACTION.md).

Before changes, N-01 passed 2/2 Release checks and R-01 passed 3/3, including
long stress. Those initial logs are in `build-research-20261115/` and
`/private/tmp/aste-{n01,r01}-baseline.log` on this machine. Historical cycle-1
measurements and their source identities were not rewritten.

## Requirement coverage

“Existing” means inspected existing implementation and fresh verification,
not new functionality invented in this pass. Interface rows are design studies,
not working native editors or accessibility approvals.

| Experiment | Executable work and remaining boundary |
|---|---|
| N1 | Existing deterministic scheduler, per-event locks/conditions/microtiming, seeded variation and partition/rate tests |
| N2 | Existing acceleration/gain/pitch mutation; **new** restart/carry/crossfade articulation and action-edge measurements |
| N3 | Existing independent cycles and immutable local fragment capture, reverse, shortening and release |
| N4 | Existing preallocated stereo master capture/retrigger; raw and matched WAVs |
| N5 | Existing nested local/master Continue/Suspend fixtures, explicit source/local/master precedence and structural void |
| N6 | Existing transport segment, seek, invalid context, tempo and buffer-partition stress, rerun here |
| N7 | Existing single-screen SVGs and keyboard/accessibility/gesture specifications; new articulation-inspector questions; no native UI |
| N8 | Existing actual Impulse and Impulse→Loop renders regenerated from the same N-01 hit trace |
| R1 | Existing equal-energy white/pink/shaped noise, impulse, granular and synthetic external excitation |
| R2 | Existing modal/waveguide comparison, tuning and CPU/storage probes, regenerated |
| R3 | Existing harmonic/stretch/inharmonic arrangements, four chord structures, cents and mode damping |
| R4 | Existing independent/coupled fixtures and autonomous energy oracle |
| R5 | Existing Reset/Shared/Partial/Overlap; **new** uncoupled/coupled retunes after only one initial excitation |
| R6 | Existing sample-offset held-note/chord fixture; current-polyphonic Sequence reference added; no live MIDI trial |
| R7 | Existing decay/persistence/release, 12 one-minute stress fixtures and restricted fold-band probe rerun |
| R8 | Existing four-original-Sequence→Field/noise→Field controls; **new** current polyphonic Sequence→textured Field and matched-noise control |
| R9 | Existing excitation/memory SVGs and interaction specification; no native UI or usability trial |

## N-01: time belongs to explicit layers

Host time H is supplied as sample position, PPQ and tempo. Between supplied
tempo/discontinuity boundaries it advances by `BPM / (60 * sampleRate)` per
sample. Phrase time P is H minus accumulated suspended beats. Local event cycles
derive from P; captured fragments advance their own sample read positions.
Tempo changes therefore affect scheduled events, not the pitch/length of frozen
sample fragments. Host sync is never silently disabled.

| Layer/action | Ownership and return rule |
|---|---|
| Event | Per-event pitch/color/decay/gain, bounded repeat train and seeded decisions; exact same-sample slot collision favors the later event |
| Local fragment | Replaces only its source; other local cycles continue |
| Master Continue | Replaces the audible mix while sources, local readers and phrase time continue underneath |
| Master Suspend | Master reader and H continue; phrase/source/local state are held, except explicit commands still execute |
| Release | Continue reveals current phrase position; Suspend resumes held phrase state at the current tempo |
| Rejoin | Explicit reset to host-aligned phrase time; invalidates capture history and may cut |
| VOID | Outermost final-output gate: no event/local/master repeat can fill it |

Commands execute before that sample's sources, in their supplied same-sample
order. Capture selects already recorded history, not future audio. Malformed or
unsorted command batches are rejected. A seek/discontinuity clears history;
tempo changes alone preserve it. A block-only API cannot reconstruct a host jump
hidden inside a block: Steinberg explicitly allows hosts not to split such
blocks ([ProcessContext](https://steinbergmedia.github.io/vst3_doc/vstinterfaces/structSteinberg_1_1Vst_1_1ProcessContext.html)).

### New N2 articulation experiment

`Articulation` is a lab-local event field, not a public parameter identity:

- Restart retains the prior reset-to-new-source behavior and 2 ms onset.
- Carry keeps oscillator phase, noise-filter state and source age while renewing
  amplitude and applying the new event's locks. It can still jump gain/color.
- Crossfade retains one outgoing source and blends for 2 ms into a restarted
  source. Storage and work remain bounded. Another hit during a fade replaces
  that outgoing tail; it is not an unlimited overlapping-voice engine.

Three identical six-hit accelerating impact fixtures share scheduling and
lock decisions. Median raw left-channel boundary delta across 65 active repeat
edges (excluding edges wholly silenced by VOID):

| Policy | Median boundary delta | Maximum boundary delta |
|---|---:|---:|
| Restart | 0.0678296 | 0.139335 |
| Carry | 0.00940764 | 0.0291593 |
| Crossfade | 0.00221550 | 0.00645242 |

These are raw amplitude differences, not level-matched perceptual click scores.
The three envelopes have different loudness/onset shapes. The listening pack
also retains matched copies. Tests prove the isolated crossfade starts on the
uninterrupted outgoing sample, reaches the exact restarted source after 2 ms,
preserves hit decisions and remains partition-independent at four rates.
The accelerated fixture's last gap is shorter than 2 ms: bounded tail replacement
is deliberately exposed, not claimed to be universally click-free. Structural
cuts remain available separately.

## R-01: excitation is energy, not the chord

The modal candidate uses four tones × three complex modes, with independent
modal tuning/damping and bounded inter-tone rotations. The alternative uses four
fractional-delay strings per bank with damping/dispersion. Excitation is a mono
energy stream, independent of harmonic structure. The resonator creates the
pitched response; Strike changes tuning/memory but does not itself synthesize
input. See [complex resonators](https://www.dsprelated.com/freebooks/filters/Complex_Resonator.html)
and [modal state-space synthesis](https://www.dsprelated.com/freebooks/pasp/State_Space_Modal_Synthesis.html).

The existing four memory policies answer different questions: Reset discards
history; Shared retunes retained coordinates; Partial damps before retuning;
Overlap keeps two independently tuned banks. Unforced orthogonal rotations do
not increase the modal norm; continuous excitation into unit-radius persistence
still requires an explicit energy bound. Orthogonal mixing is not a guarantee
of bounded driven energy ([lossless feedback matrices](https://www.dsprelated.com/freebooks/pasp/Choice_Lossless_Feedback_Matrix.html)).
The DAFx [coupled filter-bank study](https://www.dafx.de/paper-archive/2023/DAFx23_paper_42.pdf)
provides a further alternative for future controlled nonlinear transfer; that
model was not implemented or represented as the current rotation model.

### New R5 and stronger R8 controls

Two new fixtures inject only the initial 40 ms burst at 0.1 s. Later chord
changes are Retune events, with no additional input after 0.14 s. The renderer
checks the later input is exactly zero. Coupling is respectively 0 or 8 Hz;
nominal T60 is 8 s. Both retain measurable late output without any energy-guard
intervention. Raw peaks were 0.05575/0.05835. This isolates transformation of
stored energy; it does not establish a preferred sound.

The new family comparison uses one current eight-voice Sequence core with the
Warm Brass values and its +3 dB final master, into Field's current Open Field
values/Texture 70. A second new reference passes the identical unpitched noise
into that Field. Old references remain as separate controls. All four reference
renders are checked at 127/511 frames, with old note-offs ordered before new
note-ons. The reference is a practical family chain, not an equivalent-control
null test: Sequence supplies its own envelopes/oscillators, and its note input
does not apply the R-01 snapshot's cents offsets. Reference guard/state metrics
remain `unknown`, not invented zeros.

Fixed linear shared-state response remains equal to superposed linear responses
in the existing negative-control test. “Memory” alone therefore does not prove
an instrument distinct from synthesis plus reverb. Retuning, partial damping,
time-varying transfer and useful repeatable gestures still have to earn that
distinction in a composition task.

## Performance, correctness and limitations

This run used AppleClang 17, arm64 macOS, C++20. Each suite passed in both Release
and ASan/UBSan: N-01 2/2, R-01 3/3. All 17 faults compiled and were detected
(7 N-01, 10 R-01). The source/OS-call audit is self-calibrating on this platform;
this is not a proof concerning every possible OS API or another platform.

Median elapsed wall time / rendered duration, sequential with builds finished:

| Workload | 48 kHz / 128 frames | 96 kHz / 128 frames | Storage at 48 kHz |
|---|---:|---:|---|
| N-01 sparse | 0.249% | 0.476% | 672,000 capture bytes + 3,456 state bytes |
| N-01 dense nested | 0.478% | 0.876% | Same fixed capacity |
| R-01 modal, two banks/coupling | 0.517% | 1.021% | 3,088 state bytes, no sample heap |
| R-01 waveguide, two banks | 0.253% | 0.500% | 96,256 sample bytes + 3,088 state bytes |

N-01's timing workload retains default Restart articulation; the new policies
have correctness/RT audit coverage, not a dedicated worst-case CPU budget.
N-01 capture rises to 2,688,000 bytes at 192 kHz; R-01 waveguide sample storage
is 384,256 bytes there. Practical research ceilings remain 1% of one thread at
48 kHz and 3 MiB N-01 / 512 KiB R-01 storage at 192 kHz. These local fixtures fit;
short wall-time trials are not DAW scheduling, oldest-hardware or Intel proof.

The C3 tuning probe reports under 0.20 cent error over six rates; it is not a
full tuning-grid result. Strong persistent stress deliberately activates guards.
In the restricted 4.5 kHz probe, waveguide base-rate folded-band power is about
−43.82 dB under strong drive versus −156.46 dB weak/decaying. The filtered 4×
comparison changes the operating point and is not pure alias subtraction.
Keep this as a diagnostic: no oversampling feature or alias-free claim is
justified. Abrupt tuning/coupling transitions need wider sideband studies.

No callback allocations/locks/I/O were introduced. No live DAW, human listening,
native accessibility, screen-reader or interaction timing test was performed.
N-01 has no importer, live gesture recorder or portable session serializer;
R-01 has no live MIDI/host/velocity/pedal adapter. Neither lab silently supplies
those missing features. Captured audio and harmonic state persistence need an
explicit future off-thread ownership/versioning design, not copied plugin IDs.

## Interface and minimal candidate definition

The existing [N-01 default](../docs/research/n01/default.svg) /
[held](../docs/research/n01/held.svg) and
[R-01 excitation](../docs/research/r01/excitation.svg) /
[memory](../docs/research/r01/memory.svg) wireframes were retained and XML-checked.
They are illustrative static compositions, not screenshots of running DSP.

N-01's smallest candidate remains three sources, event locks/repeats, three
independent cycles, local capture, one master fragment, explicit Continue /
Suspend / Release / Rejoin, and visible void. Put articulation in the selected
event's inspector, not another permanent control bank. Label the time owner and
return destination. Typed positions/lengths and keyboard alternatives must work
without dragging; momentary Hold must release on focus loss. Undo edits the
score, not undocumented audio history. Existing interaction specifications cover
focus order, scaling, progressive detail and gesture recording proposals.

R-01's smallest candidate remains modal-led, four tones/three modes, explicit
excitation and retune actions, decay/damping, one bounded transfer control and
Reset/Shared/Partial memory. Keep Overlap, waveguide and current Sequence→Field
as comparisons. Show actual per-bank modal energy, not decorative FFT bars;
waveguide has four measured line energies, not twelve fabricated modes. A
retune must not silently excite. Blank/no-energy and rejected-note states are
as important as sounding states.

Reject eight elaborate N-01 drum engines, an imported-sample browser, a universal
product engine, permanent modal/waveguide “quality” switches, a modulation matrix
and plugin integration at this stage. Shared event-lock vocabulary is useful;
Impulse/Loop timing and memory ownership differ, so extracting a common runtime
would erase the very distinctions being tested.

## Reproduce and listen

Run from the repo root into a **new** directory:

```sh
python3 research/integration/reproduce_labs.py \
  --output build-research-cycle2-new --cmake /path/to/cmake --mutations
```

This Mac additionally required `--sdk /Library/Developer/CommandLineTools/SDKs/MacOSX15.5.sdk`
and `--cxx-include /Library/Developer/CommandLineTools/SDKs/MacOSX15.5.sdk/usr/include/c++/v1`.
The runner builds/tests both configurations, generates two packs independently,
compares every pack file, validates WAV headers/durations, emits SHA256SUMS,
runs measurements/mutations, times sequentially and writes a receipt. It refuses
existing output and rejects source changes during a run. That refusal was tested.

The completed local directory is `build-research-cycle2-20261115/`:

- `N01-listening.zip`: 20 variants × raw/matched = **40** stereo float32 WAVs,
  48 kHz, 16 s each.
- `R01-listening.zip`: 32 variants × raw/matched = **64** stereo float32 WAVs,
  48 kHz, 8 s each.
- `research-source.zip`: corresponding source inputs and license, with original
  relative paths; receipt contains its SHA-256.
- Each `listening-a/` includes blank responses, obscured IDs, answer key, raw
  outputs, traces/measurements, source fingerprints and SHA256SUMS. Each
  `listening-b/` is the independently rendered byte-identical check.

The [retained receipt](../docs/research/cycle2/evidence/receipt.json) and
[evidence index](../docs/research/cycle2/README.md) preserve numerical results;
large generated WAVs/ZIPs are ignored build products. RMS targets are 0.06 for
N-01 / 0.05 for R-01, subject to peak caps; this is not perceptual loudness
matching. Listen to obscured files before revealing names; retain raw files to
judge envelope/gain differences. The response sheets are blank. No audition or
musical assessment is represented as completed.

The next decision gate is a matched composition/repetition task: reproduce a
sparse phrase, predict a time/memory gesture, change it deliberately, undo it
and reproduce the result. Compare N-01 with Impulse/Loop and R-01 with current
Sequence/Field. Rate identity, control, space/legibility, editing effort,
return predictability and preference separately. “No useful difference” is a
valid reason to stop or extend existing products instead of shipping another.
