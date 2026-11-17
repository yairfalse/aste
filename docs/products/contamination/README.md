# Contamination C-01

Contamination is a **MIDI instrument**, listed as **Contamination C-01** in
Cubase’s VST instruments. Play notes or chords; it does not require audio input.

New instances start with Submerged Chord. Version 0.11.0 adds a dense source,
nonlinear resonant filtering and interacting, degrading audio feedback. Existing
sessions retain Classic; choose USE UPDATED SOUND to adopt the new circuit with
your current controls. The original four snapshots select Classic; five new
sound-target snapshots select the updated circuit.

Start with Submerged Chord or Corroded Stab. In Classic, Pressure changes the oscillator
return and voice integrators; Contamination adds correlated noise inside that
return. Instability introduces slow frequency movement. Memory retains
excitation-dependent operating state between notes. Motion shapes phase depth
and spectral opening; Colour sets the return’s spectral centre. The envelope
provides short strikes through long held textures. Master Output controls the
final clean level independently of those sound controls.

- **CC1 / modulation wheel:** additional phase depth.
- **Channel aftertouch:** additional pressure.
- **Sustain pedal and pitch bend:** handled by the instrument.
- **Clear Memory:** discharge accumulated operating state while held notes keep
  sounding.
- **All Sound Off:** clear notes, tails and memory.

Project state saves the ten sound controls, Master and Bypass. Current sounding
voices and accumulated operating state are not serialized; reopening starts
with fresh live state. DAW stop clears sounding state. The editor shows measured
stored state, oscillator-return activity and final output.

## Integration

The normal root build includes C-01 alongside the family. Version 0.11.0,
bundle `invalid.aste.contamination-c01`, manufacturer `Aste`, code `Cnt1`, VST3
category `Instrument|Synth`. The existing standalone candidate identity,
12-parameter order and schema-1 `contamination-c01` state remain intact.

Host indices 0–9 are `pressure`, `contamination`, `instability`, `memory`,
`motion`, `colour`, `attack`, `decay`, `sustain`, `release`; 10 is final `master`
(-36..+18 dB), and 11 is `bypass`. Existing IDs/order are retained; future controls
append. The product adapter remains under `apps/contamination`; the classic
framework-independent engine remains under `research/c01`; the revised circuit
is under `apps/contamination`. No IDs or parameter slots were added.
The `circuit` state property selects classic (absent/1) or revised (2); invalid
revisions reject the whole state. Stored audio is not serialized.

BODY/PRESSURE, CORROSION, DRIFT, FEEDBACK and EROSION/MOTION are musical labels
for the existing primary controls. Feedback stores actual audio in the revised
circuit; envelope Release governs voices independently. Clear Memory fades the
cleared return over 5 ms, while All Sound Off clears immediately.
See the [redesign and listening protocol](../../../research/c01-redesign/README.md).

The prepared sample-rate policy uses internal oversampling according to the
existing measured budget. The plugin reports **16 samples of latency** to the
host. This is checked explicitly rather than assuming the zero-latency contract
of other Aste products. The universal build contains arm64 and x86_64 slices.

See the [original design and measurements](../../research/c01/RESEARCH.md),
[parameter/state contract](../../research/c01/CONTRACT.md) and
[original evidence](../../research/c01/EVIDENCE.md). Musical listening in Cubase
remains a separate check from the build and suitable-host tests.

## Installed verification — 2026-11-16

0.10.0 was installed in the system VST3 folder. All four selected integration
checks passed: C-01 engine, C-01 adapter/editor, C-01 VST3 host, and Grain host
regression. The installed C-01 passed separate native arm64 and Intel/x86_64
host loads, 12 parameters, 16-sample latency, state recall and finite output
across six block sizes. Strict/deep code-sign and universal-slice checks passed.
The minimum-size editor image was visually inspected.

The initial new host check probed latency before DSP preparation; it was moved
after prepare so it verifies the valid 16-sample contract. C-01 DSP was not
changed. Local logs, editor image, source hashes and installed receipt are under
`build-evidence/contamination-playable/`. Other installed plugins were unchanged.

## Host-freeze fix — 0.10.1

0.10.0 could repeatedly request host reconfiguration: the shared prepare path
set latency to zero and C-01 then restored 16. A host reacting to those changes
by preparing again could loop indefinitely. The shared path now preserves
product latency; C-01 advertises 16 at construction and retains it across all
prepare calls. No DSP or parameter identity changes.

A bounded reprepare-host test reproduced 15 alternating notifications over
eight preparations before the fix; after the fix it settles after one with
zero notifications. Further rate/block changes and the zero-latency base case
are covered. The prior smoke host checked only the final latency and missed
this feedback loop. This is a reproduced protocol defect; the original Cubase
thread stack was unavailable after force-quit.

The fixed 0.10.1 bundle was restored to the system plugin folder on 2026-11-17.
Installed arm64 and Intel host checks pass; its binary hash matches the tested
fixed build. The before/after restart logs and seven passing checks are retained
in `build-evidence/contamination-freeze-fix`. Actual Cubase retry remains to be
confirmed by the musician.
