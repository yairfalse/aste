# Sequence and Impulse musical power

Repository source inspected at `8a603dc`, 2026-11-14. This is a source and
actual-editor-artifact review, not a listening session. The creator's report
of insufficient power is substantive negative musical evidence. Numerical
claims below are either explicitly identified derivations or prior repository
results; new production/alternative measurements belong to the parent audit lab.

## Does Sequence act like an expressive playable synthesizer?

### Takeaway

The architecture can supply a broad monophonic bass/lead palette, but there is
a high-confidence MIDI articulation defect before any taste-based argument
about oscillator/filter character: new notes during an old release are treated
as legato. Pressure also combines rising drive with attenuation and cutoff
ceiling effects, so monotonic internal coefficients cannot establish increasing
musical force.

### Cited Findings

- The source is two polyBLEP saw/pulse/sine morphs with complementary pulse
  widths, a sine sub at half the fundamental, and a shared nonlinear four-stage
  low-pass. Oscillator 2 can move ±12 semitones; the sub is mixed at up to 0.55
  amplitude before nonlinear loading. Both output channels are identical.
  [Processor, lines 344–363 and 404–435](../../apps/sequence/sequence_processor.cpp).
- Pressure maps to mixer drive `1 + 4.5p`, envelope-depth multiplier
  `1 + .65p`, accent gain `.12 + .38p`, and filter loading `p`. But mixer output
  divides by `max(1, .72*drive)`, filter compensation divides by the square root
  of input gain, and the output is bounded by tanh. More Pressure therefore
  increases several mechanisms while reducing intermediate amplitude; the
  net response must be measured at fixed pitch/contour and matched levels.
  [Mapping and compensation, lines 156–178 and 413–429](../../apps/sequence/sequence_processor.cpp).
- Filter Weight linearly interpolates stages two and four of the same state
  graph; feedback reads stage four for every Weight. This replaces a rejected
  parallel-filter morph whose mismatched phase/gain could reduce body at the
  midpoint. The new graph removes the old topology, not the need to evaluate
  perceptual weight, resonance, bass loss or intermediate cancellation.
  [Current graph, lines 143–168](../../apps/sequence/sequence_processor.cpp);
  [ADR 0012](../../docs/adr/0012-sequence-shared-state-character-filter.md).
- A **new non-overlapping keyboard note can fail to retrigger its attack**.
  `handleMidi` passes `envelope_.active()` as `slide`; `active()` includes the
  Release stage. `Envelope::noteOn(false)` changes nothing unless idle. A new
  note received after note-off but before release reaches idle therefore stays
  in release while its frequency and velocity change. This is a control-flow
  fact, with the magnitude/audibility to be rendered by the audit lab.
  [MIDI path, lines 252–277](../../apps/sequence/sequence_processor.cpp);
  [note-on, lines 80–84](../../apps/sequence/sequence_processor.cpp);
  [active definition, line 123](../../apps/sequence/sequence_processor.hpp).
- Direct MIDI only supports notes and all-notes/all-sound-off. Pitch bend,
  aftertouch, CC and sustain pedal are ignored by the adapter. Released-note
  fallback chooses the highest remaining note and starts it at velocity 1,
  rather than retaining that note's original velocity. This makes expressive
  MIDI performance shallower than the large synth surface may suggest, but
  absence of a modulation matrix is not by itself a defect.
  [Adapter, lines 669–693](../../apps/sequence/sequence_plugin.cpp);
  [held-note and fallback, lines 225–231 and 268–276](../../apps/sequence/sequence_processor.cpp).
- The same ADSR drives amplitude and cutoff. Cutoff is multiplied by
  `2^(5*envelope*envAmount*(1+.65p))`, then bounded to 0.42 Fs. At maximum
  Pressure this is an 8.25-octave requested excursion, not an unconditional
  five-octave result. Bright settings can spend much of a note at the ceiling.
  Velocity and accent are applied **after** the filter, so they do not alter
  the filter-envelope trajectory or exciter loading. The Accent tooltip's
  "filter emphasis" is not implemented as a distinct filter response.
  [DSP, lines 147–148 and 417–429](../../apps/sequence/sequence_processor.cpp);
  [tooltip, lines 255–256](../../apps/sequence/sequence_plugin.cpp).
- The built-in program uses sixteen fixed steps, a 78% gate fraction unless
  sliding, binary accent, and absolute host PPQ. MIDI transposes a running
  program at subsequent step boundaries; it plays the synth directly when
  stopped/disabled. This supplies repeatable composition, but no explicit
  gate-length editing or rhythmic variation beyond gate/accent/slide.
  [Scheduler, lines 291–341](../../apps/sequence/sequence_processor.cpp).
- Smoothed voice controls are mostly 10–20 ms; Output is 5 ms. Detune and
  envelope parameters are read directly, not through those smoothers. Thus
  "automation is smoothed" is not a statement about every control or a
  sample-accurate host automation stream: parameters are read once per plugin
  callback. [Preparation/process, lines 181–203 and 375–406](../../apps/sequence/sequence_processor.cpp);
  [plugin processing, lines 713–724](../../apps/sequence/sequence_plugin.cpp).
- Existing tests call scalar coefficient growth "musically monotonic".
  The Weight test compares RMS at 0/50/100%, accepts a midpoint above one quarter
  of the weaker endpoint, and requires only a nonzero RMS difference. Those
  checks are useful regressions but neither isolates spectral depth nor
  establishes musical monotonicity. The MIDI partition test has only one
  onset, so it cannot catch release/retrigger behavior.
  [Tests, lines 72–103 and 128–174](../../tests/sequence_tests.cpp).
- Four presets are defined. "Open Form" includes a +14-semitone step even
  though step parameters permit only −12…12; this is an out-of-range preset
  definition deserving an adapter test, not proof that +14 actually reaches
  DSP. [Preset, lines 98–104](../../apps/sequence/sequence_plugin.cpp);
  [range, lines 570–573](../../apps/sequence/sequence_plugin.cpp).
- Actual minimum-size editor artifact inspected at
  `build-r01/worktree/build-plugin/sequence_editor_artifacts/sequence-minimum.png`
  from the unchanged product code: 18 equal-size knobs, three clear groups,
  readable note-name fields, a visible keyboard/sequence cue, and all sixteen
  steps. Pressure has no stronger visual priority than ordinary controls;
  Wave displays only a percentage. Pitch slider travel is very short at the
  960×600 minimum, although typed note names provide precision. The source
  confirms the equal-size three-group layout and 24-pixel switch rows.
  [Layout, lines 438–473](../../apps/sequence/sequence_plugin.cpp);
  [step interaction, lines 223–297](../../apps/sequence/sequence_plugin.cpp).
- State schema 2 stores 85 scalars and migrates schema 1 with fixed Pulse Width
  and Filter Drive defaults; prior IDs remain stable. The current product
  report explicitly leaves bass weight, high-resonance listening, slide
  semantics and real-project mix placement unfinished.
  [State contract](../../docs/products/sequence/STATE_FORMAT.md);
  [music-machine gate, lines 22–24](../../docs/products/sequence/MUSIC_MACHINE_TEST.md).

### Inferences

- **Highest-priority experiment:** retain the entire production graph and alter
  only MIDI legato detection in an isolated copy: true overlap means a note was
  held before the incoming note-on, not merely that an envelope tail exists.
  Compare repeated same-pitch and changing-pitch phrases with 20 ms gaps,
  genuine overlapping notes, and long gaps. Include envelope evidence, per-note
  onset/tail RMS, level-matched audio and a no-change legato control. This can
  improve deliberate articulation without adding one knob. It still changes
  existing MIDI renders, so shipping requires an explicit compatibility policy.
- **Second research hypothesis:** Pressure may narrow usable drive/contour
  regions rather than simply add force. Separate measurements of fundamental
  energy, harmonics, crest factor and matched timbral residual should map
  Pressure × Weight × cutoff × envelope amount. Do not remove compensation or
  tanh merely to make the candidate louder; first identify which cancellation
  is perceptually unwanted.
- **Expressive-depth hypothesis:** post-filter-only velocity/accent may make
  soft and hard playing mostly a level change. A later controlled experiment
  could move a bounded amount of existing velocity influence into excitation
  or contour depth, preserving matched output. This is a changed performance
  law, not an automatically justified feature addition.
- UI should reveal waveform landmarks, effective contour saturation, and the
  relationship of Pressure to the filter. First correct the inaccurate Accent
  explanation. A separate full modulation page would evade the simpler causal
  issues and is not recommended.

### Gaps

- No new audio or CPU measurement is claimed in these notes. Parent laboratory
  must reproduce the keyboard defect and quantify parameter sensitivity before
  ranking alternatives as measured improvements.
- No human judgment of oscillator weight, filter personality, fatigue or mix
  usefulness has been supplied beyond the creator's negative overall feedback.
- The artifact review is actual plugin painting, not interactive Cubase,
  VoiceOver, Retina or automation-write validation.

## Do Impulse's eight objects offer useful depth and control?

### Takeaway

Impulse's rhythmic surface has more depth than its per-object synthesis models.
The eight objects are not eight interchangeable samples, but several share one
exponential envelope, a very small oscillator/noise construction and the same
saturation stages. At least one advertised timbral control is inactive, and
some sounds change time scale with sample rate; widening ranges alone would
not correct either problem.

### Cited Findings

Every voice is one preallocated object per track. Retrigger replaces phase,
amplitude, noise/filter state and sound parameters, rather than overlapping or
crossfading tails. Pitch, decay, Tone, Drive and Level are **latched at trigger**.
Changing them during an existing tail does not edit that voice, while global
Energy's final saturator and Output continue to change. This semantic boundary
is important for automation responsiveness.
[Trigger, lines 60–83; output, lines 223–231](../../apps/impulse/impulse_processor.cpp).

| Object | Actual source construction | Specific limitation to investigate |
|---|---|---|
| Kick | Sine, initial pitch up to 3.8× root, brief noise edge scaled by Tone | Fixed `.992` per-sample pitch decay; Tone only affects the short initial edge, not body |
| Click | Unbandlimited fixed 8%-duty pulse with negative level −0.12 | Tone is a pre-saturator amplitude factor, not a filter/shape change; high pitches alias |
| Burst | One-pole low-passed deterministic recurrence plus 0.25× sine at 1.37× pitch | Fixed normalized filter coefficient changes bandwidth with sample rate; one shared amplitude decay |
| Body | Sines at 1 and 1.618×; Tone changes second-component amplitude | Inharmonic pair, not an independently damped resonant body; both partials share envelope |
| Low | 0.9× sine with initial pitch up to 1.85× plus a fixed noise edge | **Tone is never used**; `.987` pitch decay is fixed per sample |
| Crack | Noise minus previous one-pole state, plus 0.18× 1.37× sine | Fixed normalized high-pass behavior; no independent crack/body contour |
| Metal | 1× and 2.731× sines plus Tone-scaled mixed phase | Third term multiplies an already wrapped phase by 1.414, causing a discontinuity at wraps; upper partials are not individually bandlimited |
| Cut | Tone-dependent narrow pulse, then first-order high-pass | Width/filter change together; unbandlimited pulse edges and one common envelope |

All source descriptions above come from
[renderVoice, lines 86–141](../../apps/impulse/impulse_processor.cpp), not
listening adjectives. Tone's common tooltip promises a bright/noisy-component
change even for Low, whose implementation has none.
[Tooltip, line 119](../../libs/parameters/plugin_ui.hpp).

- Pitch-decay time constant is mathematically `−1/(Fs*ln(a))` for coefficient
  `a`. Kick's `.992` is approximately **2.594 ms at 48 kHz versus 0.648 ms at
  192 kHz**; Low's `.987` is **1.592 versus 0.398 ms**. These are derived
  constants, not measured listening differences. Main amplitude decay does
  correctly use sample rate. Noise/filter coefficients are likewise fixed
  normalized values. [Decay, lines 75–77; source coefficients, lines 110–136](../../apps/impulse/impulse_processor.cpp).
- All voices pass through `tanh(sample*(1+5*drive))`; Energy adds up to 0.35
  to each event's drive, then applies another tanh to the summed mono signal
  and a level divisor. Dense simultaneous events share this second nonlinear
  stage. That can create useful glue or erase distinctions/transients; neither
  outcome should be assumed. The side component bypasses that common tanh
  before final bounds. [Drive, lines 79–80, 139–141 and 216–235](../../apps/impulse/impulse_processor.cpp).
- The noise recurrence is a bounded affine map followed by fraction extraction,
  not a conventional PRNG or guaranteed white/pink process. Its autocorrelation,
  repeat length and spectral coloration have not been established by the
  current tests. [Noise update, lines 99–101](../../apps/impulse/impulse_processor.cpp).
- MIDI event variation seeds use `seed XOR sampleIndex`, where sampleIndex is
  **callback-local**, not absolute stream time. The scheduler instead hashes
  seed, track, absolute step and tick. Thus sequencer partition invariance does
  not establish MIDI partition invariance. Existing tests compare partitioned
  sequencer renders, while MIDI tests use one 127-frame buffer and trigger only
  first/last objects despite the assertion's "all eight" wording.
  [MIDI/scheduler seeds, lines 163–171 and 197–200](../../apps/impulse/impulse_processor.cpp);
  [tests, lines 35–60 and 69–89](../../tests/impulse_tests.cpp).
- Sequencing offers independent 1–32-step lengths, 1–4 ratchets, ±49% timing,
  conditional cycles, probability, seed and mutation. Mutation flips activity
  after the ordinary condition check, so it can create a hit on a conditionally
  inactive step. Ratchets retrigger the same voice; there is no overlapping
  voice allocation or per-cell sound lock. These are actual semantics, not
  reasons to redesign it into N-01.
  [Scheduler, lines 177–212](../../apps/impulse/impulse_processor.cpp).
- The current plugin's `starter()` explicitly replaces the core's historical
  polymetric defaults with variation 0, common length 16, full probability,
  and Kick/Click/Crack patterns. A raw `Parameters{}` lab render is therefore
  **not the current plugin default**. The existing `impulse_lab` still uses raw
  core defaults, which can misrepresent first-use behavior in research.
  [Starter, lines 37–56 and parameter construction 493–510](../../apps/impulse/impulse_plugin.cpp);
  [old lab, lines 15–25](../../apps/dsp-lab/impulse.cpp).
- Five factory starting points reset all parameters, but after reset their
  differences principally set Energy, Variation, Mutation, sequence state,
  lengths, pulses and patterns—not different per-object sound designs. The
  preset browser therefore showcases rhythmic variety more than the eight
  engines' timbral range. [Preset code, lines 710–765](../../apps/impulse/impulse_plugin.cpp).
- Actual minimum-size artifact inspected at
  `build-r01/worktree/build-plugin/impulse_editor_artifacts/impulse-minimum.png`
  from unchanged source: all 256 cells visible, inactive half-grid dimmed in
  the starter, object sound controls below, and a distinct Energy row. Small
  step labels and cells compete with large global space. Every track selection
  also auditions, so selecting controls cannot be silent in a playing session.
  No human interaction result is implied by the screenshot.
  [Selection, lines 350–356; geometry, lines 410–438](../../apps/impulse/impulse_plugin.cpp).
- Explicit generation rather than hidden pulse-count rewriting and predictable
  schema-3 defaults are useful responses to earlier feedback. The retained
  368-parameter host surface and schema-aware migration keep visible patterns
  and recall aligned, but do not prove convenient Cubase automation navigation.
  [ADR 0013](../../docs/adr/0013-impulse-visible-patterns-and-eight-objects.md);
  [ADR 0016](../../docs/adr/0016-playable-defaults-and-tape-recall.md);
  [state contract](../../docs/products/impulse/STATE_FORMAT.md).

### Inferences

- Prioritize truthful control response and repeatability before eight new
  engines. Low Tone can be exposed as inactive in a backward-compatible UI
  correction; giving it a real DSP action needs explicit opt-in behavior or
  migration because saved Tone values currently have no sound effect.
- A narrow fixed-48-kHz-reference time-law alternative would preserve the
  current reference sound while expressing pitch envelopes and one-pole
  coefficients in seconds/Hz at other rates. Compare all rates with identical
  musical timing, not simply "finite at six rates." This is a useful engineering
  correction, but may not by itself make the 48 kHz instrument more compelling.
- A bounded onset/body split for one selected object is a more meaningful
  architectural experiment than widening every shared Pitch/Decay range.
  Choose the object after independent matched listening, not from names.
  Keep current voice identity and all eight existing objects as controls.
- MIDI seed ownership must be independent of callback partition. Options
  include explicit absolute sample position or a documented per-track event
  ordinal; the latter needs reset/seek semantics. This is deterministic behavior
  work, not a new randomization feature.
- Separate silent selection from intentional audition, and label trigger-latched
  sound edits. Improve step grouping and keyboard movement before exposing more
  automation parameters. The audition change is UI-local; DSP/state risks are
  low if its existing bounded request handoff remains intact.

### Gaps

- No independent listening result yet establishes that any object is convincing,
  strange, powerful, redundant or aesthetically weak. The table intentionally
  describes algorithms rather than claiming those judgments.
- Need per-object pitch/decay/Tone/Drive sweeps, low/high velocity, sparse versus
  simultaneous events, and nominal-time cross-rate comparisons. Noise recurrence
  periodicity and Metal phase discontinuity need spectral evidence; merely
  measuring HF energy would not isolate aliasing.
- Actual plugin MIDI octave mapping is in the adapter; core test events must
  use canonical notes 36–43. Core and plugin defaults must not be conflated.

## Which changes merit priority and what must remain protected?

### Takeaway

Sequence's missing fresh attacks is the strongest narrow experiment from these
two products. Impulse's dead/misleading control and partition-dependent MIDI
randomness should be measured as correctness/controllability failures before
speculative feature expansion. Neither product requires abandoning its identity.

### Cited Findings

- Both products keep framework-independent fixed-state DSP, sample-offset MIDI
  scratch arrays, bounded output and no callback allocation; those are genuine
  safety constraints. The respective schemas and stable IDs are compatibility
  boundaries. [Sequence header](../../apps/sequence/sequence_processor.hpp);
  [Impulse header](../../apps/impulse/impulse_processor.hpp);
  [real-time policy](../../docs/REALTIME_SAFETY.md).
- Zero latency, a fixed 16-step Sequence program, eight single Impulse voices,
  zero oversampling and provisional local CPU ceilings are design choices or
  product boundaries, not reasons a control must be dead or a new MIDI attack
  must be suppressed. Prior rejected features were rejected for insufficient
  demonstrated need, not a physical impossibility.
  [Superseded Sequence ADR 0008](../../docs/adr/0008-sequence-voice-and-clock.md);
  [current ADR 0012](../../docs/adr/0012-sequence-shared-state-character-filter.md);
  [Impulse ADR 0013](../../docs/adr/0013-impulse-visible-patterns-and-eight-objects.md).
- Real host/first-use gates remain explicitly open, including note articulation,
  sub translation, rapid automation and high-frequency aliasing.
  [Sequence music-machine protocol](../../docs/products/sequence/MUSIC_MACHINE_TEST.md);
  [Impulse music-machine protocol](../../docs/products/impulse/MUSIC_MACHINE_TEST.md).

### Inferences

| Priority | Change/experiment | Expected musical impact | Confidence and engineering effort | Compatibility |
|---|---|---|---|---|
| 1 | Sequence held-note-based retrigger versus production | Reliable deliberate attacks and keyboard articulation | High code-level confidence; small isolated change, full transition tests required | Existing MIDI renders change; no silent production replacement |
| 2 | Impulse MIDI partition-invariance investigation | Reproducible bounce and sound choice | High code-level cause; small/medium timing-policy work | Changes seeded MIDI timbres; preserve old behavior explicitly if shipping |
| 3 | Impulse Low Tone truthful affordance or isolated meaningful mapping | Removes blind experimentation/dead knob | Exact unused read; UI correction small, DSP mapping needs listener evaluation | UI-only neutral; DSP reinterpretation has session risk |
| 4 | Sequence Pressure/Weight/contour gain and sensitivity map | More purposeful macro motion without loudness bias | Medium hypothesis; measurements first, not another drive increase | New mappings change renders; candidate/version boundary required |
| 5 | Impulse reference-rate time laws and one-object onset/body study | Stable attacks across rates; potentially more independent timbral depth | Rate law high confidence; musical architecture medium/low until listening | Reference-rate law can preserve 48 kHz; other-rate old renders differ |
| 6 | Silent track selection; waveform/latched-edit feedback | More deliberate performance and fewer misleading actions | High implementation confidence; usability benefit still untested | UI-local, stable parameter IDs retained |

The acceptance task should ask a musician to repeat a specific articulation or
object transformation, explain what changed, and recreate it after recall.
Blind matched audio should establish preference or useful difference separately
from speed/error counts. A louder result, greater RMS separation, more harmonics
or passing tests alone is insufficient.

### Gaps

- Ranking across all six instruments belongs to the combined audit, after the
  other researchers' findings and current measurements are considered.
- The source-review phase made no production changes. Candidate acceptance and
  later measured findings are distinguished from that original inspection.

## Executed follow-up: Sequence host gate partition failure

### Takeaway

The unchanged production host sequencer has a reproducible floating-point
gate-boundary disagreement across callback partitions. This is separate from
the MIDI release-aware candidate and must remain an explicit failing baseline
gate, not an expected-failure test or an undocumented harness adjustment.

### Cited findings and reproduction

`apps/sequence/sequence_processor.cpp:309` reconstructs PPQ as block-origin
PPQ plus local sample time; lines 312–314 derive the step by division/floor;
lines 337–340 stop a non-slide gate when fractional phase reaches 0.78.
An independent scratch C++ driver linked unchanged `libpower_products.a`, used
the lab's rational 25 ms automation boundaries, and rendered eight seconds at
48 kHz/120 BPM using maximum blocks of 127 versus 511. Transport stops at six
seconds, matching the common laboratory score. It made no production edits.

At absolute sample 16,680 (0.3475 seconds), the relevant callback origins are
16,616 and 16,622. The exact production expression yields:

| Quantity | Maximum block 127 | Maximum block 511 |
|---|---:|---:|
| Step position | 2.7800000000000002 | 2.7799999999999998 |
| Fractional phase | 0.78000000000000025 | 0.7799999999999998 |
| Gate threshold `phase >= 0.78` | true | false |
| Default audio at sample 16,680 | 0.071267895400524139 | 0.071317307651042938 |
| Fast-automation audio at sample 16,680 | 0.13962699472904205 | 0.13972260057926178 |

The first unequal audio sample is exactly 16,680 for both recipes. Default
stereo difference RMS is `8.9790649285282334e-06`; fast-automation difference
RMS is `0.035236864968920999`. There are 30 opposite raw gate-threshold
predicates within the six-second running interval, though inactive gates and
slide steps make some ineffectual. This trace found no disagreement in the
integer step index.

As a causal diagnostic only, adding `1e-12` PPQ to both callback origins makes
both partition pairs bit-identical (`difference RMS = 0`). This perturbation
is **not a proposed production fix**, is not applied to listening-pack
renders, and does not establish a general boundary policy. It demonstrates
that floating-point boundary placement, rather than different programmed
automation, explains this reproduction. The production clock expression and
strict equality gate remain intact. The new lab's existing-host automation
test is consequently not all-green.

### Limits and next step

This result establishes the specific offline rate/tempo/score/partition case,
not an actual Cubase failure or subjective audibility. The much larger
fast-automation divergence is measured; its nonlinear propagation should not
be generalized to every phrase. Research an explicit sample-boundary policy
that also handles tempo changes, seeks and negative PPQ. Keep default and
automated cases, several partitions and sample rates as regression witnesses;
do not replace exact comparison with a convenient tolerance or silently bias
host PPQ. Any adopted timing correction needs a compatibility decision.
