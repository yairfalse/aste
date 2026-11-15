# Aste musical-power audit: second pass

Historical audit findings are retained below. The [playability follow-up](../playability/README.md)
records the subsequent fixes, current evidence and remaining musical acceptance.

2026-11-15. Audited product source: `73760000a12b5a22638548d04f7d50ecaf205eb2`,
including the click/sub follow-up. This pass changes **no production code,
presets, parameters, state formats or installed plugins**. No PR or release.

**Verdict: no, the collection is not musically accepted yet.** The revision
repairs several real control failures, but it does not demonstrate six complete
sound identities. This pass also found a reproducible remaining Impulse timing
defect that the passing standard tests miss. Do not equate a functional receipt
with either musical acceptance or a release pass.

The creator confirmed that Cubase runs on **another computer**. The development
Mac's old installed Density is therefore not evidence of what the creator heard.
The music machine's exact plugin builds, sessions and behavior modes remain
unknown. The creator's original complaints are substantive musical evidence;
there is still no human preference result for the revised audio.

## Confirmed findings, ordered by consequence

### 1. Impulse triplet retriggers depend on callback partitioning

**Correctness defect; high confidence; release-blocking for a deterministic
rhythm claim.** In the existing Debug/ASan/UBSan `impulse_tests` executable,
changing only Click's `ratchet` to 3 makes the existing exact comparison fail:

```text
FAIL: Impulse output is independent of block partitioning
Process exited with status = 1
```

[Full debugger reproduction](evidence/impulse-triplet-debugger.log).
The comparison is the existing two-second, 48 kHz, 120 BPM fixture: constant
127-frame callbacks versus the repeating 1/7/127/511 pattern. The debugger
changes an ordinary parameter in memory before rendering; it does not patch
DSP, assertions or source. The unmodified test passed in the full suite.

The scheduler independently recomputes PPQ inside each callback and floors
`shifted / (stepDuration / ratchet)` at
[the processing boundary](../../../apps/impulse/impulse_processor.cpp).
Unlike Sequence's repaired clock, it does not keep a continuous sample anchor.
With ratchet 3 the subdivision is not exactly representable in binary.

An independent Rust arithmetic witness compares 127/511-frame partitions at
six rates, four tempos, all four ratchets and zero microtiming for eight seconds.
It finds a first differing tick in **18 of 96 configurations**, all ratchet 3.
At 48 kHz/120 BPM, sample 14000 yields tick 6 versus 7. This is a tick witness,
not a claim that this sample is the first differing audible hit in the test.
[Witness rows](evidence/clock-witness.csv). The 96-case sweep is arithmetic,
not 96 production-audio renders; the debugger test establishes the audio failure.

The scheduler expression is unchanged from the original production baseline:
this is an inherited defect, not caused by the new click/sub presets. Both new
presets use ratchet 1. Earlier MIDI seed and Sequence gate fixes do not solve it.
The standard Impulse partition fixture uses default ratchets; its six-rate test
uses ratchet 4 but checks finite output, not cross-partition equality.

**Next:** promote this reproduction to a permanent regression, then investigate
one sample-owned Impulse clock. Cover every ratchet, timing offsets, host loops,
seeks, stop/restart, tempo changes and conditions. Do not add a broad PPQ epsilon
or relax sample equality. Decide saved-session timing compatibility explicitly;
the existing legacy/current switch alone does not distinguish two revisions of
current schema-4 scheduling. No repair is silently made in this audit.

### 2. Density has more nonlinear participation, not proven better articulation

**Measured tradeoff; high confidence in mechanism, preference unknown.**
The direct detector change is real: matched percussion still differs from the
old detector with the same controls after removing a best-fit scalar gain
(residual **−13.146 dB** relative to the current signal). Bass gives **−16.145 dB**.
It is not merely an output-level adjustment. A residual is not a quality score.

The independently inspected raw bass fixture spends **6.58%** of its samples
within 1% of its peak at the new start, versus **1.13%** for the same controls
with legacy detection. The extreme profile reaches **69.49%**, and its crest
factor is only **1.38 dB** over the full eight seconds. These counts include
the fixture's two-second silent tail; they describe flattening, not an audibility
threshold or a measured hard-clip percentage.

Fresh spectral diagnostics reproduce the tradeoff at 48 kHz, −18 dBFS input:
996 Hz H3 changes from **−25.620 to −16.893 dBc**, while the folded fifth of
7004 Hz at 12980 Hz rises from **−49.590 to −31.918 dBc**.
[Spectral evidence](evidence/density-spectral.csv). This is one identified fold,
not total alias energy. The existing 1x topology was not changed in this audit.

**Next:** compare useful transient/body contrast at matched level before adding
drive. Revisit the retained envelope/nonlinear alternatives only with a concrete
musical task. Do not promote oversampling merely because a fold is measurable;
evaluate its existing CPU/latency tradeoffs against listening.

A separate small usability defect remains: the shared Drive tooltip says
“Push the input into compression and saturation,” although current Density
detects the un-driven input. Legacy and current modes need accurate wording.
[Tooltip source](../../../libs/parameters/plugin_ui.hpp).

### 3. Clicks/subs is a starting vocabulary, not yet a complete performance design

**Design limitation; source and audio evidence, musical preference unknown.**
Fresh renders of both new scenes are byte-identical to the previous export;
all 12 scene/rate checks pass. The scenes establish short attacks and low bodies
without an eight-engine rewrite. Existing measured isolated energy durations
and settled low-frequency spectra are in the [click/sub report](../click-sub/README.md).

Remaining boundaries in the actual processor:

- Each track has one voice. A retrigger resets its amplitude and phase rather
  than overlapping another body. Sound Level/Pitch/Decay/Tone/Drive are copied
  at trigger time, not continuously applied to an existing ringing object.
- Low still has the original pitch transient (initial frequency multiplier
  1.85), an onset edge and saturation. Drive 0 is not a linear-path switch:
  Energy contributes to voice drive and the summed signal has another `tanh`.
- Pitch-envelope factors (`0.992`/`0.987`) and noise-filter coefficients are
  per-sample constants, so transient character is not time-invariant across
  rates. Click/Cut remain hard-edged pulse sources, not newly bandlimited engines.
- All eight new-pack WAVs are exactly mono in stereo, consistent with Variation
  0. This is intentional for these fixtures, not evidence of absent stereo capability.
- Eleven empty event positions do not mean eleven silent time slices. On the
  complete matched eight-second renders, 272/800 Clicks/sub and 196/800
  Cuts/pressure ten-millisecond windows fall below −60 dBFS combined-channel
  RMS. These counts include the tail and depend on matching gain; they are not
  an active-pattern silence percentage or a listening threshold.

The [actual minimum-size editor](../click-sub/impulse-minimum.png) still gives
eight rows × 32 cells and global randomness controls substantial visual space.
Only one selected object's sound controls are visible at a time. The quieter
starting pattern and explicit audition are useful, but the workflow can still
feel like configuring a large drum machine to get two objects.

**Next:** fix triplet timing first. Then evaluate repeated sub attacks and
click/body contrast in sparse composition, including whether active-object
focus makes deliberate editing faster. Do not replace all engines or add an
N-01 temporal architecture under the Impulse name.

## Status of all six instruments

| Instrument | What is actually stronger or clearer | Remaining limitation | Smallest meaningful next evaluation |
|---|---|---|---|
| Density | Drive is no longer counteracted by driving detection; raw and matched waveforms confirm a change | Stronger saturation can flatten contrast and increase folds; macro interactions still need musical acceptance | Matched percussion and bass: reach glue, pressure and transient/body contrast deliberately, not merely higher density |
| Harmonic | Color eligibility is visible; PUSH/PULL exposes input operating level | DSP is unchanged: all seven same-control source comparisons are exact audio nulls. New start sets Harmonic to 100%. At Input +12/Output −15, only one +3 dB PUSH fits before Output reaches −18; another PUSH is a no-op, without a disabled-state cue | Separate contour from color on quiet/loud sources; test useful range, then improve limit feedback before redesigning topology |
| Sequence | Release-aware attack and sample-anchored host gates have regression evidence | Oscillators/filter/shared envelope are unchanged; fixing articulation does not establish an original voice. Velocity scales output plus accent selection rather than continuously scaling filter-envelope depth | Play separated/overlapping MIDI at varied velocities and compare Pressure/Weight at matched output; do not infer expressiveness from monotonic mapping tests |
| Loop | Intended motion phase now continues; tape snapshots already survive recall | Capture/print/playback still have different laws. At empty capture, default Overdub 50% stores half the input; 100% stores silence. Feedback acts during capture, not passive playback. Pitch/Reverse interaction is unchanged. Old tape states restore legacy motion | Capture once, predict level, print three generations and recover them. Clarify endpoint meanings and version behavior before changing gain/pitch laws |
| Impulse | Sparse Click/Low start, explicit audition, Low Tone response and MIDI event identity | Confirmed triplet timing defect; single-voice resets, trigger-latched controls, unchanged transient/sample-rate laws and eight-row interface | Triplet regression first, then a sparse clicks/subs performance task rather than more objects |
| Field | Hold no longer secretly raises three color controls; labels no longer claim total buffer energy | Still a lossy transforming FDN; zero Motion is not perfectly static. State saves controls, not held audio. A newly reopened session can show hold with an empty field | Excite, hold, evolve, release and reopen a real phrase; distinguish retained parameters from retained sound before proposing audio-memory recall |

Harmonic's null is an expected control, not a failed product. Sequence/Loop/Field
time-domain residuals can be large because timing or network state diverges;
they must not be ranked as “more character” using that number.

## Interface and Cubase workflow review

Re-inspected all six actual JUCE minimum-size renders and corresponding source:
[Density](../quality/screenshots/density-minimum.png),
[Harmonic](../quality/screenshots/harmonic-minimum.png),
[Sequence](../quality/screenshots/sequence-minimum.png),
[Loop](../quality/screenshots/loop-minimum.png),
[Impulse](../click-sub/impulse-minimum.png),
[Field](../quality/screenshots/field-minimum.png).
These are existing component captures of the audited implementations, not new
Cubase screenshots. The Density picture is a metered test fixture, not its default.

The family language is coherent and controls fit. Remaining priorities:

1. **Reliable identity on reopen.** Shared `PresetMenu::remember` records the
   chosen name and values only inside the open editor. Fresh/reopened editors
   do not identify the constructor's starting scene or recalled preset. The
   asterisk tracks modifications only after a menu choice in that editor.
2. **Behavior compatibility must be understandable.** Sequence shows LEGACY;
   Field identifies its hold mode. Density's generic footer does not identify
   which response is active. Sequence and Loop have no in-place opt-in upgrade;
   loading old Loop tape state deliberately restores old motion behavior.
3. **Show useful boundaries.** Harmonic's PUSH/PULL limit needs feedback; Loop's
   Capture/Overdub/Feedback and Record/Loss roles need clearer grouping. An
   ineffective control can feel weak even if the underlying DSP is active elsewhere.
4. **Test actual performance, not just component presence.** Compact Sequence
   toggles and Impulse cells still need keyboard/precision/VoiceOver observation
   on the music machine. No new accessibility signoff is claimed.

Documentation has drifted: `docs/UI_SYSTEM.md` still describes five Density
presets and Field's previous hold coloration; the implementation and the newer
product/state reports describe the revised behavior. Record this as documentation
debt, not authority to revert the implementation.

No Cubase automation, offline/freeze bounce, session reopening or native
assistive-technology session was performed. The local standard install paths
were checked read-only: only Density 0.1.0 was found, unlike the new build.
The creator's separate music machine is the relevant next host target; its
installed versions and whether projects select legacy response are still unknown.

## Executed evidence and what it does not prove

- Native Release build succeeded. Full CTest: **67/68 pass**, **exit 8**, sole
  failure `dependency_security`, review expired 2026-11-07.
  [Full output](evidence/full-ctest.log). No waiver or date edit.
- Fresh independent reference comparison: **144 exact legacy stereo matches**
  against original `8a603dc`; loudness-meter oracle passed.
  [Output](evidence/legacy-check.log).
- Fresh six-product export: **356 WAVs**, byte-identical to the earlier pack's
  audio; only provenance README/additional listening guide differ. Its Impulse
  profile intentionally remains historical Space, not the new default.
  Fresh current-Impulse export: **8 WAVs**, byte-identical to the prior click/sub
  repeat pack. These are rendered, not human-assessed.
- Independent Rust inspector validated all **364 float32 WAVs**, reports peak,
  RMS, adjacent-sample step, near-peak occupancy, quiet windows and mono identity:
  [six-product scan](evidence/six-product-scan.csv),
  [current Impulse scan](evidence/click-sub-scan.csv).
  Adjacent-sample jumps are not a click or aliasing detector.
- **90 gain-fitted waveform comparisons** over 30 product/source groups:
  [data](evidence/shape-comparisons.csv). The least-squares scalar removes gain,
  not timing, phase or EQ. It is separate from the renderer's LUFS matching.
- **72 refreshed Density spectral rows**; no new CPU, true-peak, memory or
  latency benchmark. Previous local performance evidence remains historical.
- **12 click/sub scene/rate checks** pass:
  [output](evidence/click-sub-check.log). They do not cover ratchet-3 host sequencing.
- Rust parser/gain tests pass; isolated mutations doubling decoded amplitude
  and halving fitted gain cause both checks to fail:
  [negative control](evidence/negative-control.log). Empty evidence is rejected,
  not treated as a complete zero-result scan. No new dependency or release crate.
- LLDB's reproduced **production-audio failure remains open**. LLDB itself
  can exit 0 even when the child exits 1; read the child status and failure text.

Methods and commands: [reproduction guide](../../../research/re-audit/README.md).
Committed console logs normalize trailing whitespace/CRLF only; original command
output remains in `build-audit2-evidence/` in the working copy.
Fresh audio remains in `build-audit2-listening/` and `build-audit2-clicks/` in this
working copy. Existing ZIPs were not overwritten. The Sykli contract for this
audit validates the inspector only; it cannot turn the open DSP failure or the
expired security check into a product pass.

## Decision and next three priorities

1. **Repair Impulse's triplet event clock**, with the failing regression and an
   explicit saved-session compatibility decision. High causal confidence,
   bounded engineering scope; no extra performance control needed.
2. **Find Density's useful pressure/articulation range** through matched musical
   tasks. Test the present direct graph before choosing an envelope or nonlinear
   alternative. High measured tradeoff, unknown preference, sound-compatibility risk.
3. **Make clicks/subs deliberately playable**, evaluating sub retrigger behavior
   and active-object editing rather than adding sounds. Moderate design confidence,
   smaller UI options before larger DSP changes; preserve the existing product identity.

Harmonic boundary feedback, Loop capture semantics, Field recall expectations
and the stale help text are concrete follow-ons, not reasons for another
simultaneous six-engine rewrite. The direction remains a more playable family,
but “we have it all” would be an unsupported claim. Acceptance requires the
creator to complete the per-instrument musical tasks on the actual Cubase setup.
