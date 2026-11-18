# Loop L-01 cycle 03: accessible generational memory

## Delivered implementation

Safe Takes adds one-shot input capture, actual waveform/region editing,
protected three-slot audio history, neutral history audition, reversible
removal/Clear, momentary two-dimensional fragment performance, automatable
repeat divisions and explicit input monitoring. Original sessions keep their
old graph. RELOOP now prints the same auditioned coloration once in Safe
Takes; printed PCM remains independent of later edits.

See [workflow and bounds](PERFORMANCE_WORKFLOW.md),
[parameters](PARAMETERS.md), [state](STATE_FORMAT.md) and
[architecture decision](../../adr/0021-loop-protected-performance-workflow.md).

## Evidence obtained during implementation

- Original six Loop CTests passed before changes.
- New one-shot/full-history checks failed against the old behavior, then passed.
- A retained-energy Undo test exposed delayed APVTS bookkeeping; explicit
  transaction flushing fixed it. PCM removal/Clear undo and redo were exercised
  through the production adapter, not just a simulated command model.
- A deliberate fault making fragment speed advance the underlying tape caused
  the release-phase test to fail. The corrected separate timeline passed.
- Core tests compare one-sample and larger buffers, exact auditioned/printed
  PCM, stereo storage, six standard sample rates and allocation counts.
- Adapter tests cover real editor gestures, exact parameter/PCM recall,
  legacy migration, concurrent state handoffs, transport/tempo jumps and
  rapid extreme automation with allocation/lock/I/O auditing.
- The native production VST3 was built and loaded by the repository smoke host
  during development. Populated and minimum-size editor images were inspected.
- Seven stereo 48 kHz/24-bit examples were actually rendered by the production
  adapter. A representative Release run took 3.8–8.1 ms per two-second example,
  including the render harness's copying/measurement overhead. These are
  single-run local observations, not a portable worst-case CPU guarantee.
  Peak/RMS were printed per file; examples preserve their actual output levels.

No human listening assessment or Cubase validation is claimed. Earlier cycle
reports are historical: cycle 02's statement that captured audio cannot be
recalled was superseded by the portable tape-state work and is not a current
limitation. Final exact-commit verification is recorded in the delivery receipt,
not inferred from an earlier build's test results.

## Remaining musical acceptance

On the creator's Apple-silicon Cubase machine, test live monitoring, host
automation write/read, repeated transformation/listening, deliberate hard
splices, stop/release and complete project save/reopen. Judge both subtle and
destructive settings at matched listening level. The three-take limit,
dual-head pitch artifacts and free-running transport are explicit current
bounds; this change does not claim an unlimited archive or transparent
time-stretching.
