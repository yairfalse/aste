# Improvement backlog and musical acceptance

Expected impact is a **hypothesis**, confidence describes the causal evidence,
and engineering effort is a relative scope estimate rather than a schedule.
Compatibility risk means risk to saved sound/behavior, not merely XML or ID
changes. This is not approval to change production or begin all items at once.
Read [the audit](../../../reports/Aste%20musical%20power%20audit.md),
[capability matrix](CAPABILITY_MATRIX.md) and [executed evidence](EVIDENCE.md).

## Selected first investigations

| Rank | Product and narrowly isolated experiment | Expected musical impact | Confidence | Engineering effort | Compatibility / scope |
|---|---|---|---|---|---|
| 1 | Sequence: distinguish a new note during Release from genuine held-note legato | Reliable attacks and deliberate keyboard phrasing | High control-flow cause; new musical preference pending | Small signal-law change; broad MIDI-transition regression required | Changes existing MIDI renders; laboratory-only, preserve genuine overlaps and sequencer slides |
| 2 | Loop: replace the prematurely wrapped one-second motion clock with a bounded common 1000-second cycle | Predictable intended slow motion without artificial one-second resets | High mathematical cause; desirable sound unknown | Small timing change; long-duration/rate/transport comparisons required | Changes motion in old sessions; no simultaneous pitch/gain/snapshot rewrite |
| 3 | Harmonic: linked +12 dB Input/−12 dB Output using existing production controls | Reveal useful coloration at quiet track levels without raising final level | High operating-point mechanism; benefit conditional on source | Small laboratory gesture; calibration and matching work | No new DSP law or stable parameter; user-applied control change, not silent preset reinterpretation |

The phrase "1000-second cycle" denotes a bounded common period of the current
0.47/6.71/0.173 Hz decimal frequencies, not a proposed extra performance control.
Selection favors direct causal questions and small falsifiable changes across
distinct failure classes. It is not evidence that these are the three products
musicians dislike most or that every candidate should be adopted. New output
levels, harmonic energy, smoother trajectories or test passes do not establish
musical benefit.

## Ranked follow-on work

| Priority | Item | Expected impact | Confidence / effort | Compatibility risk and smallest next step |
|---|---|---|---|---|
| High | Sequence host gate-boundary ownership across callback partitions | Repeatable host-sequenced bounces and fast automation | Demonstrated 48 kHz/120 BPM failure: 127/511 maximum blocks disagree at phase 0.78; small/medium scheduler research | Correct sample ownership can alter old renders. Keep strict failing baseline gate; define boundary/seek policy, verify across rates and tempos, and do not hide the issue with PPQ bias or relaxed audio tolerance |
| High | Impulse MIDI seed independent of callback partition | Repeatable MIDI audition, offline bounce and buffer changes | High source cause; small/medium timing-policy work | Existing seeded MIDI renders change; choose absolute-time/event-ordinal ownership explicitly and retain legacy behavior if needed |
| High | Impulse Low Tone truthfulness | Remove a dead musical gesture | Exact unused control; small UI correction or medium DSP study | Mark inactive without sound change; any new Tone law reinterprets saved values and needs opt-in/versioning |
| High | Field energy/retention label or measurement correction | Predictable hold status and release | High source mismatch; small/medium bounded-meter work | UI/meter-only can preserve audio; do not label a tap proxy total stored energy |
| High | Harmonic inactive/low-drive cue; Loop capture-only Feedback and Overdub endpoints | Reveal existing capability and avoid ineffective moves | High dependency facts; small UI/text study | No sound changes; validate minimum-size density and screen-reader wording |
| High | Impulse silent selection distinct from audition | Prevent extra hits while inspecting controls during performance | High interaction cause; small UI change | Preserve an explicit accessible audition gesture and bounded request handoff |
| High | Confirm installed build and reproduce creator's failed gestures | Avoid fixing an old revision or the wrong source-level problem | Essential missing context; small diagnostic task | Read-only project/build evidence; no reset or overwrite of sessions |
| Medium | Density matched Drive/Crush/Density sensitivity and recovery map | More independent, purposeful pressure/transient control | Strong overlapping laws, musical limitation unproven; medium analysis | Diagnose before remapping; preserve both gain and raw dynamic evidence |
| Medium | Revisit retained Density detector/oversampling finalists under current graph | Useful articulation or cleaner extreme character | Existing implementation evidence, preference missing; medium research | New detector/latency paths alter renders; reevaluate target-machine budget instead of deleting safety |
| Medium | Sequence Pressure/Weight/cutoff/contour interaction and velocity placement | Greater timbral responsiveness without another modulation page | Medium musical hypothesis; medium research | New mapping/velocity law changes sound; first fix tooltip and map saturation plateaus |
| Medium | Impulse reference-rate time laws; Metal phase and pulse/noise diagnostics | Stable attacks across rates and useful high-frequency extremes | High time/phase mechanisms, preferred sound unknown; medium work | Preserve 48 kHz reference where feasible; other-rate old renders still change |
| Medium | Loop capture gain and Record reproduce/print law comparison | Predictable generation energy rather than confusing attenuation/growth | High source law, preference unknown; medium research | Keep current capture and every generation as controls; do not silently normalize tapes |
| Medium | Loop signed Speed/Reverse/Pitch tests | Reach intended transposition and edits deliberately | Specific derivative hypothesis; medium measurement/algorithm work | Any corrected pitch law changes saved combinations; no automatic old-state migration |
| Medium | Field current bundled hold versus retention-only hold | More intentional persistence/evolution choices | High hidden coupling, musical benefit unknown; medium research | Preserve damping, bounds and input policy; do not add a switch before selection |
| Medium | Reopen-aware preset identity and Custom state | Easier session navigation and comparison | High editor-local metadata cause; small/medium UI work | Prefer exact parameter matching/metadata; audio state need not change |
| Medium | Impulse grid / Sequence steps keyboard and precision study | Faster performance editing, fewer errors | Actual small targets; benefit needs observation; medium interaction work | Preserve full-pattern overview, stable IDs, exact entry and accessibility |
| Research | Harmonic shelf-region nonlinear ownership / serial versus parallel residual | More distinct useful color across the boosted region | Plausible architecture mismatch; larger research | Preserve contour, phase, neutral/cut behavior and rejected residual-clipping control |
| Research | One Impulse object's independent onset/body shaping | Deeper timbral variation without eight replacement synthesizers | Requires per-object listener selection; medium/larger study | Preserve identity and old engine; no N-01 temporal redesign |
| Research | Field held-material snapshot recall or different return intervals | Reproducible long performances / wider harmonic contexts | Workflow boundary clear, sonic need unproven; larger state/DSP work | Explicit versioned state, bounded buffers and off-thread restoration required |

## Acceptance gates by product

| Product | A musician's acceptance task | Objective support and falsifiers |
|---|---|---|
| Density | Reach a restrained glue setting and a forceful transient/body transformation on percussion and dense music; repeat it without an output-level advantage | Retain raw gain/GR/recovery/crest data, externally matched comparisons and dry null. Reject a candidate whose only advantage is louder output or one-source tuning |
| Harmonic | Deliberately produce broad contour versus audible nonlinear color on quiet and loud sources, then return to a clean contour | Compare linked gain against 0/0, individual/multiple boosts, flat/cut/Harmonic-zero controls, IMD and folded-content diagnostics. Reject if contour meaning or overload recovery is sacrificed |
| Sequence | Play separated and overlapping phrases, predict attack/glide/accent behavior, then reproduce them via MIDI and the built-in program | Fresh-note versus legato envelope tests, per-note attack evidence, matched phrase files and no-change slide controls. Reject if new attacks break genuine legato or if apparent improvement is only increased level |
| Loop | Capture once, create three meaningfully different generations, predict a transport gesture and reopen the intended result | Raw/matched generational files, capture gain, pitch/motion traces and recall boundary checks. Reject arbitrary degradation or a nicer sound that cannot be repeated/recalled deliberately |
| Impulse | Create clearly differentiated useful objects and a repeatable sparse/dense groove, then edit a sound without accidental hits | Independent object sweeps, control nulls, MIDI/PPQ partition tests, cross-rate timing and source-specific alias diagnostics. Reject eight renamed variations of one outcome or hidden random instability |
| Field | Build a space, arm/fill hold, predict its evolution, release or clear it, then recover the performance in a session | Impulse/chord/noise/sustained excitation plus silence, decay/stereo/line-state traces, raw/matched hold comparisons and truthful snapshot limits. Reject duration alone as evidence of depth |

Each task needs a listener/performer record: source material, source level,
settings, host/build, sample rate/buffer, desired outcome, actual failure or
success, confidence, and whether the result repeats. Record time, edits and
errors separately from sound preference. Include unfamiliar source material,
not only the fixture that motivated a candidate. No new human acceptance result
is asserted here. A negative or indistinguishable outcome is a useful result
and should retain the production behavior.

## Compatibility and release boundary

Keep IDs, state layouts, preset interpretation and production DSP unchanged
during this audit. A future sound-changing candidate needs a separately
approved migration/opt-in strategy, old/new render fixtures, documented default
behavior and host recall validation. Fixing a technical defect does not make
its change sonically backward-compatible. Label-only improvements must still
preserve accessibility and automation interaction.

Real-time ownership, finite bounded state, no audio-thread allocation/locks/I/O,
validated recall and honest latency are non-negotiable. Specific local CPU and
latency targets can be challenged with measurements and an artistic case, not
silently removed. Existing release/security/host gates remain independent of
the musical acceptance gate; neither one substitutes for the other.

Source grounding:
[Density/Harmonic](../../../research_notes/Aste%20musical%20power%20audit/density_harmonic.md),
[Sequence/Impulse](../../../research_notes/Aste%20musical%20power%20audit/sequence_impulse.md),
[Loop/Field](../../../research_notes/Aste%20musical%20power%20audit/loop_field.md),
[actual interfaces](INTERFACE.md), [real-time policy](../../REALTIME_SAFETY.md).
