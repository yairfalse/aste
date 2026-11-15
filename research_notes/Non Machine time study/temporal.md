# N-01 host timing and temporal alternatives

Research notes, 2026-11-14. Repository observations refer to baseline
`8a603dcbc59b52a58637fe6305676efc5a406f80`. These are design proposals, not claims
about the concurrently developed prototype. Public documentation was retrieved
on the date above; source HTML is temporary, ignored build material, not an
implementation dependency.

## What can host synchronization actually guarantee?

### Takeaway

Use separate validity-bearing host and instrument clocks. Promise exact event
placement against an explicitly supplied tempo/position segment, not knowledge
of transport changes the host never reports.

### Cited Findings

- VST3 supplies timing per processing block. Hosts may split blocks at cycle
  jumps, but are not required to; a fixed-block host may leave an internal jump
  unreported until the following block. Musical position, tempo, cycle bounds,
  and continuous sample time have separate validity flags. Project sample time
  is always valid when a `ProcessContext` is supplied; musical position is in
  quarter notes. — [Steinberg ProcessContext](https://steinbergmedia.github.io/vst3_doc/vstinterfaces/structSteinberg_1_1Vst_1_1ProcessContext.html)
- JUCE explicitly warns that not every host/format supplies every timing field;
  most `PositionInfo` getters return an optional. — [JUCE PositionInfo](https://docs.juce.com/master/classAudioPlayHead_1_1PositionInfo.html)
- Impulse currently converts absent BPM/PPQ to `120.0`/`0.0` in its adapter. Its
  core derives ticks from PPQ per sample and resets tick memory when the
  deviation from expected PPQ exceeds `max(0.02, increment * frames * 2)`.
  These are existing product policies, not a reusable universal clock contract.
  — [Impulse adapter](../../apps/impulse/impulse_plugin.cpp),
  [Impulse core](../../apps/impulse/impulse_processor.cpp)
- VST3 parameter queues contain sample offsets and piecewise-linear values;
  constant values need not be retransmitted, and the point at sample zero can
  be omitted. A discontinuous automation jump uses consecutive old/new points.
  — [Steinberg IParamValueQueue](https://steinbergmedia.github.io/vst3_doc/vstinterfaces/classSteinberg_1_1Vst_1_1IParamValueQueue.html)
- Aste's DAW compatibility record still labels Cubase and Ableton music-machine
  runs as pending; validator evidence is not a Cubase interaction or automation
  result. — [Host compatibility](../../docs/HOST_COMPATIBILITY.md)

### Inferences

Proposed adapter boundary:

- Carry `playing`, valid PPQ, valid tempo, optional project-sample position,
  and an explicit discontinuity marker; reject non-finite values before integer
  conversion. Unknown PPQ must not look like bar one. An explicitly selected
  free clock is a separate mode, not a silent fallback.
- Host time `H` owns synchronization; phrase time `P` owns pattern decisions;
  sample-age/read indices own already captured material. UI wall-clock timers
  own none of these. Ordinary operation has `P = H - phraseOrigin`.
- For a constant-tempo segment, `H(n) = H0 + n * bpm / (60 * sampleRate)`.
  Quantize each event to the first sample at or after its ideal beat position;
  this makes error nonnegative and less than one sample. Use half-open blocks
  `[start, end)`: an event exactly at the end belongs to the next block.
- If a tempo ramp is supplied explicitly, integrate/invert that ramp. If only
  block-start BPM is available, piecewise-constant interpolation is an
  approximation. Never label its ramp behavior sample-exact to an unseen host
  tempo map. Re-anchor to valid host PPQ rather than accumulating block error.
- Detect seeks using project-sample continuity where available, and PPQ
  consistency with a documented tolerance otherwise. A large buffer-dependent
  tolerance can hide short seeks; a too-small PPQ-only tolerance can mistake a
  tempo ramp for a seek. An offline harness should provide an explicit seek
  marker so it can distinguish scheduler correctness from adapter inference.
- On seek/loop wrap/stop: choose and test a reset policy. The smallest predictable
  research policy is cancel active transformations and pending retriggers,
  discard partial captures, clear voices, and derive new event identities from
  the new position. Do not synthesize every missed event as a catch-up burst.
- Zero-frame calls do not advance clocks or consume gestures. An offset equal
  to the block length is not applied early. Stable tie ordering must survive
  different block partitions, including one-sample and irregular blocks.
- Negative microtiming requires looking into the next nominal step before it
  begins. An event at step beat `b` with shift `-d` occurs at `b-d`, not at `b`
  after discovery. Signed floor/modulo semantics need tests around preroll and
  cycle zero. Seek directly into a cell should not trigger its already-past
  onset; only onsets in the new process interval are due.
- Snapshot parameter locks when an event is accepted. Later base-control edits
  must not silently change that event's remaining repeats unless explicitly
  defined as a live performance operation. Avoid block-rate parameter snapshots
  for gestures claimed to be sample-accurate: a future JUCE adapter must prove
  whether and how host queue offsets reach the core.

### Gaps

No Cubase run, JUCE adapter prototype, tempo-ramp host recording, or native Intel
measurement was performed for these notes. Exact hidden intra-block jump recovery
is impossible from the documented block context alone. Retrieved documentation
describes interfaces, not what any particular Cubase version supplies.

## Who owns time when repeats are nested?

### Takeaway

Make repetition hierarchical but non-recursive: events retrigger sources, local
fragments replace individual voice outputs, and master fragments replace the
post-voice mix. The master determines whether underlying phrase evolution
continues or suspends; it never rewinds the host.

### Cited Findings

- Impulse already owns fixed voices, independent cycles, ratchets, conditions,
  microtiming and deterministic position-keyed variation. Its visible patterns
  are the sole ordinary playback source. — [Impulse specification](../../docs/products/impulse/SPECIFICATION.md),
  [Impulse parameter contract](../../docs/products/impulse/PARAMETERS.md)
- Loop has three preallocated tape decks. RELOOP records transformed playback
  into another generation; this is not a phrase scheduler with defined master
  suspend/rejoin semantics. — [Loop specification](../../docs/products/loop/SPECIFICATION.md)
- Aste explicitly keeps product-local timing semantics until identical behavior
  justifies extraction. — [Architecture](../../docs/ARCHITECTURE.md)

### Inferences

Competing policies worth audible A/B experiments:

| Choice | Behavior | Benefit | Cost |
|---|---|---|---|
| Continue / replace | `P` and voices evolve under master fragment playback; release reveals current phrase | Host phase never drifts; strongest predictable live return | Events occur inaudibly; UI must show shadow position |
| Suspend / resume | Freeze `P` and underlying voice state; release resumes at held phase, maintaining the accumulated host offset | Literal temporal interruption; material is not skipped | No longer phase-aligned to arrangement until explicit rejoin |
| Suspend / rejoin | Freeze during hold, then jump `P` to current `H` at release or next phrase boundary | Returns to arrangement on a declared boundary | Skips material; release is not continuation from the held point |
| Accelerated catch-up | Compress missed phrase time until phase aligns | Could make expressive elastic time | Burst/overload risk; more rules and harder prediction; reject from minimal scope |

Recommended research semantics: implement Continue and Suspend/Resume as distinct
experiments, expose the host/phrase phase difference, and add an explicit Rejoin
gesture at a known master boundary if phase alignment is required. Alternatively
select Suspend/Rejoin as the simpler performance contract, but name it clearly.
Do not call resume and rejoin equivalent. Human listening must choose between
these alternatives; neither is mandated by the APIs.

Proposed precedence and ownership:

1. Transport discontinuity cancels stale capture/repeat work before any same-sample
   instrument action. A master-level release acts before a new same-sample start;
   remaining ties use stable gesture IDs, never container iteration accidents.
2. A sound event owns a finite retrigger train. Its identity is fixed independently
   of blocks. Derive repeat onset intervals as `I[k] = I0 * a^k`, where `a < 1`
   accelerates; bound repeat count and minimum interval. Amplitude decay,
   pitch progression and timbral mutation are separate laws, not side effects
   of changing the interval.
3. A local capture taps the raw voice after its event synthesis but before local
   replacement. Local playback replaces that voice's output, not its scheduler.
   In Continue mode, event tails and local capture can evolve underneath. No
   recursive retriggering of the replayed audio and no implicit feedback.
4. Master capture taps the sum after local replacements and before master
   replacement. Thus a local repeat can be printed into the master fragment,
   but master playback cannot capture itself accidentally. This ordering is a
   musically meaningful routing decision, not just an optimization.
5. Master playback owns the audible output while active. Local and event repeats
   cannot punch through unless an explicit mix policy is introduced. Reject
   multiplying event × voice × master repeat counts into recursively scheduled
   events: the levels operate on different things.
6. In Suspend mode freeze the explicitly selected underlying graph, including
   its envelopes, local playback heads and capture writers. Freezing only its
   sequencer while tails decay is another mode; do not introduce it accidentally.
   Master fragment read time continues so the hold remains audible.
7. Release does not replay hidden events. Continue reveals the current graph;
   Resume reveals preserved graph state; Rejoin clears/reconstructs it according
   to the documented seek policy. These yield different tails at the boundary.

Seeded decisions should key on `(seed, voice ID, logical cycle, event ID,
repeat ordinal, decision kind)` rather than sample offset inside a block or
the number of RNG calls. Use distinct domains for acceptance, pitch, timbre and
sample noise. Probability zero must reject unconditionally; probability one
must accept unconditionally. Pattern voids must stay void unless explicitly
edited; a mutation control must not populate designated structural silence.

Reproducible event decisions do **not** guarantee path-independent output after
a seek. Audio fragments depend on captured history, and resonant tails depend on
prior events. Exact render repeatability requires identical initial state,
source audio, transport/gesture history and sample rate. A restored empty capture
cannot truthfully reproduce a previously held fragment merely from its seed.

### Gaps

No perceptual preference has been established for Resume versus Rejoin, gated
versus decaying suspended tails, crossfaded versus cut releases, or mono versus
stereo local captures. These are experiments and listening questions, not accepted
product decisions. The parent's prototype may intentionally implement only a
subset and must identify it separately.

## What constraints should bound the prototype and its evidence?

### Takeaway

Reuse Aste's testing and offline-output conventions, not entire product graphs.
Keep capture size, event density, state lifetime and unvalidated behavior visible.

### Cited Findings

- Every callback must be `noexcept`, preallocated and bounded, with no allocation,
  release, locks, I/O, logging, lazy initialization or system calls. Existing
  runtime interposition measures a targeted set of OS calls and calibrates its
  hooks deliberately. — [Real-time safety](../../docs/REALTIME_SAFETY.md)
- General DSP state budget is 256 KiB and default CPU target is below 1% of one
  performance core at 48 kHz/128 frames; Impulse has a documented 2% exception,
  and Loop snapshots reserve roughly 88 MiB at 48 kHz. These exceptions are not
  automatic permission for N-01 to inherit them. — [Performance budgets](../../docs/PERFORMANCE_BUDGETS.md)
- Loop's state stores finite bounded float32 audio, source sample rate, active
  deck and generation IDs; immutable snapshots and off-thread restore protect
  audio ownership. — [Loop state](../../docs/products/loop/STATE_FORMAT.md),
  [Real-time safety](../../docs/REALTIME_SAFETY.md)
- Aste's lab has little-endian float WAV writers and deterministic fixtures;
  policy requires falsifiable comparisons and explicitly separates successful
  numerical tests from perceptual quality. — [Lab](../../apps/dsp-lab/main.cpp),
  [Research policy](../../docs/DSP_RESEARCH.md),
  [Listening protocol](../../docs/testing/LISTENING.md)

### Inferences

- Capture memory should be calculated before selection: float32 bytes are
  `4 * sampleRate * (voiceCount * localSeconds * localChannels + masterSeconds * 2)`
  per bank. A history bank plus a frozen bank doubles this. For three mono
  half-second local captures and one two-second stereo master, two banks require
  2,112,000 bytes at 48 kHz (about 2.01 MiB), or 8,448,000 bytes at 192 kHz.
  These are design arithmetic, not measured prototype allocations. A seconds
  cap needs an explicit budget exception; a fixed-sample cap shortens its
  musical duration at high rates.
- Freeze capture by bounded bank ownership or a preplanned bounded copy. A
  one-off multi-megabyte copy inside a callback may allocate nothing yet still
  miss its deadline. If frozen history stops rolling, signal the reduced history
  availability rather than pretending capture remained continuous.
- For smooth splice experiments, compare a short boundary window/crossfade
  against a deliberately destructive cut. A fade changes level and short-grain
  articulation; it is not inherently a musical improvement. Test joins, reverse,
  one-sample/minimum spans, shorten while active, empty capture, and capacity wrap.
  Window length must not erase very short fragments without visible feedback.
- An envelope restart and audio-fragment restart should be separate comparisons:
  the former resynthesizes an event; the latter repeats an exact captured texture.
  Compare geometric acceleration against equal intervals using the same source,
  event count and documented gain policy.
- First evidence should include exact event logs across block sizes, a reference
  onset calculation, deterministic hashes of rendered PCM, explicit silence
  intervals, bounded maximum work, six sample rates, transport/tempo boundary
  cases, measured allocation counts, sanitizers, and CPU/state-byte reports.
  Audio equality should compare raw samples before WAV writer clipping.
- Benchmark the worst nested active case and release/seek/capture boundaries,
  not just sustained DSP. Record hardware, compiler, build type, sample rate,
  buffer, source revision, workload, median and maximum observed block time.
  Wall time from a busy shared Mac is local evidence, not a real-time guarantee.
- Keep experimental fixtures versioned and private to the lab. Do not reserve
  plugin IDs, public parameter IDs, installed bundles or a session schema yet.
  A future state decision must distinguish pattern/gesture state from captured
  audio and ephemeral transport position; silent empty-memory restore would
  change a composition built around a captured fragment.
- Minimal N-01 differentiation is hierarchical audible-time editing with clear
  silence and return semantics. More drum engines, tape aging or a second
  eight-row sequencer would duplicate existing Aste features without testing
  that hypothesis.

### Gaps

These notes contain no N-01 benchmark results, listening judgments, allocation
measurements, spectrum/alias analysis, or session-compatibility proof. Imported
audio decoding, anti-aliased variable-rate playback, persistent captured-audio
state, and GUI/host integration remain separate implementation questions.
