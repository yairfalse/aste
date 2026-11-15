# Aste musical-control revision

2026-11-15. Implementation branch `improve/musical-control`; original production
reference `8a603dcbc59b52a58637fe6305676efc5a406f80`. No PR, main merge, installation,
release or product version change. This follows the completed musical-power
audit on `research/musical-power-audit` (`7cdd7d1`), not the N-01/R-01 proposals.

The direction is fewer obstacles to intentional sound, not six larger feature
sets. User feedback identifies excessive activity in Impulse and insufficient
impact in Density. That is substantive musical evidence; the measurements below
identify mechanisms but do not establish that the new sounds are preferred.
No human audition or Cubase session test has been performed for this revision.

## What changed

| Instrument | Limitation addressed | Implemented change | Remaining question |
|---|---|---|---|
| Density | Drive also drove detection, partly compressing away its own increase | New response detects un-driven input; Drive pushes the nonlinear audio path. New `Grip` starting point, full wet with independent output trim | Is the resulting density useful after matching, or too flattened/aliased? |
| Harmonic | Color requires positive band gain and sufficient level, but the UI hid that dependency | Explicit color status, paired PUSH/PULL operating-level gestures, new `Charged Presence` start. DSP unchanged | Does the existing topology offer enough distinct band interaction? |
| Sequence | A new note during release was treated as legato; host PPQ rounding changed gate edges between buffers | Retrigger when no note remains held; continuous sample clock anchored to valid host PPQ, re-anchored on tempo/seek/restart | Does recovered articulation make the voice sufficiently expressive? Oscillator/filter identity is unchanged |
| Loop | Motion phase reset every second although its frequencies did not complete cycles | Wrap at their common 1000-second period, with original modulation depths/frequencies | Does coherent movement help generational playing? No new degradation algorithm |
| Impulse | Busy starting experience; editing could sound a hit; Low Tone did nothing; MIDI variation depended on callback offset | Two-object/three-hit `Space` start, silent selection, explicit AUDITION, progressive RHYTHM DETAILS, effective Low Tone, deterministic per-track MIDI ordinals | Do the eight existing sources offer enough character once the interface is quieter? |
| Field | FOREVER secretly increased Grain, Pitch and Motion; meters implied more than they measured | Independent hold color, `Still Field` start, FIELD ACTIVITY and RETENTION TARGET labels, truthful hold state | Is persistence expressive enough? The network remains lossy and is not a lossless freeze |

No host parameters, modulation matrix, source engines, effects, oversampling
mode or dependencies were added. Core DSP remains framework-independent C++20.
Plugin boundaries remain JUCE. Existing bounds, feedback protection, callback
audits and zero reported latency are retained; no processing allocation, lock,
wait, logging or filesystem operation was introduced.

## Compatibility is part of the change

| Product | New state schema | Old schemas restore | Preset behavior |
|---|---:|---|---|
| Density | 2 | 1 -> legacy | Original five retain values/legacy; appended Grip selects current |
| Harmonic | 1, unchanged | Unchanged graph | Original six retain values; appended Charged Presence |
| Sequence | 3 | 1–2 -> legacy | Existing presets inherit the session behavior |
| Loop | 4 | 1–3 -> legacy | Existing presets inherit behavior; schema-3 tapes retained |
| Impulse | 4 | 1–3 -> legacy | Original five retain values/legacy; appended Space selects current |
| Field | 2 | 1 -> legacy | Original five retain values/legacy; appended Still Field selects current |

New schemas require root `behavior="0"` or `behavior="1"`; it is not an
automatable sound parameter. Missing, malformed or unsupported values reject
the entire restore without changing current state. Legacy behavior survives
save/reopen. Loop owns behavior on the same engine as tape state, including
pending restores and prepare/recreation. IDs, ranges, host reset defaults and
old missing-parameter migration values are unchanged. New-instance presets
are deliberately distinct from parameter-reset defaults.

Old binaries reject the new schemas. Retain original project copies when
switching binaries. Backward recall has automated evidence; this is not a claim
of backward opening of newly saved projects, nor actual Cubase validation.
The legacy switches are not exposed as additional performance knobs.
To try current behavior, use new instances. Grip, Space and Still Field also
select it in their respective existing instances. Sequence and Loop presets
inherit the recalled behavior: there is no in-place upgrade action in this
revision. In particular, an old Loop tape state keeps its old motion clock;
loading it into a new instance intentionally restores legacy behavior too.
Opt-in migration of existing recorded tapes remains a workflow limitation.

## Correctness evidence and negative controls

- Full native Release build includes all six VST3s, real JUCE adapters, headless
  editor rendering and repository VST3 smoke-host checks: **67/68 CTest checks
  passed**. The sole failure is `dependency_security`: review expired on
  **2026-11-07**. No date was advanced and no security threshold was waived.
- **144 eight-second stereo comparisons** (six products × four profiles × six
  supported rates) are bit-identical to independently compiled original DSP
  when legacy behavior is selected. The injected +0.01 sample negative control
  exits 1 with `Legacy audio differs from pinned production`.
- New regression tests failed against their original implementations before
  fixes: Sequence release-note attacks and partition-sensitive host gates;
  Loop's truncated motion cycle; Impulse Low Tone and MIDI partition dependence;
  Density Drive/detector coupling; Field's hidden hold color.
- Sequence compares exact output at 127/511-frame partitions, automation
  boundaries, three tempos, all six supported rates, a negative-PPQ seek and
  tempo change. Real held-note legato remains intact. Host continuity uses a
  half-sample discrepancy threshold, not a permissive gate/audio epsilon.
- Loop's independent discrete-sine oracle runs four seconds at six supported
  rates plus 8 kHz, and **1002 seconds at 8 kHz** to cross the actual common wrap.
  The playhead error bound is 0.02 samples.
- Five adapters test the behavior flag through real binary XML, byte-stable
  legacy save/reopen, and transactional rejection of missing/2/-1/1.0/true/NaN.
  UI tests cover silent Impulse selection, explicit audition and paired Harmonic
  host-normalized gestures. Existing callback/state fuzz checks remain enabled.

These finite tests do not establish equality for every old session, transport
arrangement, platform or floating-point implementation.

The declared `research/quality/sykli.json` gate passed the 67 functional checks,
all six core suites with AddressSanitizer/UndefinedBehaviorSanitizer, and the
144-reference comparison with both sanitizers. Its receipt is deliberately
functional-only, not a security/release pass. Frozen-tree verification and its
exact-commit checks are recorded in `build-evidence/final-receipt.json` and
`build-evidence/final-verify.json` in the working copy. The dependency-security
gate was also rerun independently and still failed solely on its expired review;
its output is `build-evidence/final-security.txt`.

### Density automation repair, not a relaxed test

The stronger graph exposed a failure in the existing automation audition gate:
log-time Attack smoothing was 0.493 dB worse than its unsmoothed reference on the
test's residual metric. Current behavior now smooths the actual attack
coefficient over 5 ms. Legacy behavior retains its original log-time smoothing;
research prototype/reference paths remain available and distinct.

The unchanged gate now measures Attack residual -74.180 dBFS against -73.816
dBFS for the reference: **0.364 dB improvement**. Simultaneous automation is
-70.943 dBFS, **24.153 dB improvement**. Drive and Blend improve 28.570/20.354 dB.
These are residual-metric results, not reported listening impressions.

The immutable historical Density golden CSVs were not rewritten. Their renderer
now explicitly selects legacy behavior. New behavior is covered separately by
production consistency, automation and direct-Drive tests. The historical
golden therefore remains a compatibility check, not approval of a new sound.

## Spectral and performance tradeoffs

[Density spectral data](evidence/density-spectral.csv): 72 measurements, current
and legacy Grip controls. At 48 kHz, -18 dBFS and 996 Hz, H3 changes from -25.620
to -16.893 dBc. At 7004 Hz the folded fifth at 12980 Hz changes from -49.590 to
-31.918 dBc. This supports stronger nonlinear participation **and increased
aliasing**, not automatic improvement. Both versions retain the original 1x
topology and zero latency. A fifth-product measurement is not total alias power.
The previous oversampling work remains rejected for integration pending a
musically justified quality/latency/CPU decision; it has not been silently enabled.

[Callback measurements](evidence/callback-cost.csv) contain 144 local trials,
run without another Aste build/render job. At 48 kHz / maximum 128 frames,
median legacy/current one-core realtime percentages are:

| Density | Harmonic | Sequence | Loop | Impulse | Field |
|---:|---:|---:|---:|---:|---:|
| 0.224 / 0.263 | 0.631 / 0.633 | 0.249 / 0.250 | 0.242 / 0.243 | 1.131 / 1.135 | 0.308 / 0.308 |

Native arm64 Release; three trials per setting, 48/96 kHz and maximum blocks
128/511. This is DSP callback work only, not end-to-end host latency, deadline
maxima, a full-memory Loop benchmark or a cross-machine budget guarantee.
See the [laboratory method](../../../research/quality/README.md).

## Interface review

These are actual minimum-size JUCE component renders, not design mockups or
Cubase screenshots. All six were visually inspected for labels and control
overlap. Density is a metered Drive 12/Density 80 fixture, not the Grip default.

| Density | Harmonic | Sequence |
|---|---|---|
| [View](screenshots/density-minimum.png) | [View](screenshots/harmonic-minimum.png) | [View](screenshots/sequence-minimum.png) |
| Loop | Impulse | Field |
| [View](screenshots/loop-minimum.png) | [View](screenshots/impulse-minimum.png) | [View](screenshots/field-minimum.png) |

Impulse retains its existing eight-object grid and family identity; progressive
detail is not a second instrument or a replacement sequencer. Harmonic's status
reports eligibility for coloration, not measured audibility. PUSH/PULL uses
equal/opposite physical input/output gestures constrained by both parameter
ranges; it is **not** automatic loudness matching. Field activity is a mono-tap
proxy; retention is a pre-loss coefficient, not actual total buffer energy or
decay time. Hold text reports the control state even when the measured tap is
silent. Keyboard/screen-reader behavior has component-level checks, not a new
native assistive-technology or Cubase accessibility signoff.

## Listening decision and next priorities

Local exports are `quality-quick-listen.zip` (12 matched WAVs, about 19 MB),
`quality-listening.zip` (356 WAVs, about 509 MB), and `quality-vst3.zip` (six
arm64 test bundles, about 19 MB). [Artifact hashes and checks](evidence/artifacts.json)
record their source commit and SHA-256 values. All archives passed integrity
checks; all six bundles passed local strict signature verification and arm64
inspection. Local ad-hoc signatures are not Developer ID signing/notarization.

The [short listening guide](LISTENING.md) names the first comparisons to try.
Two complete exports from source commit `6ef0612` produced **356 WAVs each**
and byte-identical audio and measurement files. The 178 measurement rows span
30 product/source groups. Every row had measurable gated loudness; the largest
within-group matched difference was below 0.000001 LU. That is repeatability of
this algorithm, not external meter certification or a human loudness judgment.
WAV inspection confirmed stereo Float32, 48 kHz and 384000 frames (8 seconds).

Use matched `audio/` files, not `raw/`, for preference comparisons. Listen to
`before_start` versus `after_start` first, then `after_start` versus
`same_controls_legacy` to separate preset and DSP changes. Loop's demonstration
uses Wow 100%, not a changed default. Sequence and Impulse use common phrases;
effects use seven generated sources. No supplied musical recording was used.

Record source, comparison, preference or no preference, and the gesture that
helped or hindered a concrete musical task. Acceptance means greater deliberate
control and useful character at matched level, not choosing the louder option.
The immediate priorities after listening are Density's nonlinear/alias balance,
Impulse's source character under sparse patterns, and Harmonic's actual band
interaction. Sequence's filter identity, Loop generation laws and Field's
lossy persistence remain research questions—not secretly rewritten DSP.

Actual Cubase automation/recall, human musical judgment, renewed dependency
security review, universal builds, signing/notarization and release approval
remain outstanding. These test builds must not be described as release-ready
or as proof that all musical limitations have been solved.
