# Per-instrument DSP investigation findings

Read [executed numbers and limitations](EVIDENCE.md) before interpreting this
summary. “Improve” below means a testable proposal, not a listener-approved
result. All production processors remain untouched.

| Product | What works | Limitation and cause | Smallest next change | Fundamental research alternative | Compatibility / complexity |
|---|---|---|---|---|---|
| Density | Strong current track-level compression, wet default, parallel timing and nonlinear stages | GR, crest and final density are not monotonic together; Drive/Crush/Density coordinate several gain and nonlinear laws | Explain effective contribution and evaluate one compensated macro trajectory under matched listening | Compare envelope recovery or nonlinear participation independently, retaining current graph | Audio-law changes alter saved sound; labels are smaller than extra controls; alias/CPU evidence required |
| Harmonic | Distinct shelf/bell contours; clean neutral/cut path; nonlinear residual has a measurable operating point | Default flat gains make hero inactive; quiet sources produce little residual; shelf contour and residual cover different regions | Clear engaged/inactive feedback; audition existing linked gain gesture | Shelf-region residual ownership or serial/parallel residual comparison | Gesture uses current IDs; any new topology needs explicit compatibility handling and contour/null controls |
| Sequence | Original monophonic architecture, visible pattern, genuine legato and wide oscillator/filter combinations | New note during release treated as legato; independent PPQ rounding defect; shared amplitude/filter envelope limits independence | Release-aware candidate restores attack; separately design an integer/phase-stable host gate fix | Only if listening justifies it, research Pressure/Weight/filter articulation rather than another oscillator menu | Both fixes can change renders; preserve old sessions, genuine overlaps and slides; no host fix promoted here |
| Loop | Actual printed generations, three-deck history, schema-3 tape recall | Wrong motion period, attenuating empty capture, divergent print/playback gain, surprising signed pitch interaction | Continuous clock candidate; clearer capture/print/playback hierarchy and endpoint labels | Isolate pitch/speed law or record transfer, one at a time | Motion changes old tape behavior; no concurrent recall rewrite; snapshot memory is significant and bounded |
| Impulse | Eight genuinely different source constructions; deep rhythms and current predictable starter | Low Tone unused; MIDI seed owns wrong time coordinate; latched controls and shared saturation obscure response; pulse folds | Repair deterministic event identity and dead-control semantics with versioned sound handling; silent selection and representative sound starts | Select one object for onset/body research after independent object listening | No eight-engine rewrite or N-01 conversion; sound changes affect saved patterns; retain fixed voices and bounded scheduling |
| Field | Substantial moving FDN and coupled spatial transformation; direct hold/clear gestures | Hold bundles spectral/time changes and losses; meter is mono-output proxy, not buffer energy; no live-energy recall | Truthful observation labels; matched hold/control comparisons | Retention-only hold, alternate return intervals or bounded energy snapshot, selected separately | A snapshot is a real state/memory design change; retain finite feedback and off-thread restoration; no lossless-freeze claim |

Detailed source investigations cover parameters, ranges, defaults, gain paths,
smoothing, modulation, rejected algorithms and constraints:

- [Density and Harmonic](../../../research_notes/Aste%20musical%20power%20audit/density_harmonic.md),
  with [colour measurement methods](COLOUR_METHODS.md).
- [Sequence and Impulse](../../../research_notes/Aste%20musical%20power%20audit/sequence_impulse.md),
  with [gate diagnostic](evidence/sequence_gate_probe.txt) and
  [note/object measurements](evidence/note_response.csv).
- [Loop and Field](../../../research_notes/Aste%20musical%20power%20audit/loop_field.md),
  with [space measurement methods](SPACE_METHODS.md).

Real-time ownership, bounds, valid state and honest latency are safety
requirements. Zero-latency topology preferences, a particular filter structure,
macro coupling and historical performance targets are challengeable design
choices, but only with measured alternatives and a musical reason. No current
safeguard was weakened. Existing rejected oversampling, residual-clipping,
filter and stored-energy candidates remain documented in their product history;
the audit does not relabel those prior decisions as newly performed experiments.

The [backlog](BACKLOG.md) ranks impact, confidence, effort and compatibility
risk, and gives a falsifiable performance task for every instrument. The
proposed Aste 2.0 direction is repair and clarification first, then selective
sound research—not a simultaneous six-product redesign.
