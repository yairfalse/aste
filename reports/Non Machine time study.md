# N-01 makes rhythmic time explicitly playable

**N-01 merits an isolated temporal experiment, not a seventh plugin yet.**
Impulse already supplies independent cycles, conditions, microtiming, ratchets
and reproducible variation; Loop already transforms captured audio. The
unproven distinction is composing with per-event sound settings, local and
master fragments, protected silence, and an understandable return from altered
time. A framework-independent C++20 prototype now models those operations with
three restrained sources and no product dependency, public parameter identity
or JUCE adapter. Static wireframes explore their visibility, not a working
editor. Numerical correctness cannot establish that the result enables better
music: the comparative listening and editing trial remains the decision gate
([Impulse contract](../docs/products/impulse/PARAMETERS.md),
[Loop specification](../docs/products/loop/SPECIFICATION.md),
[experiment evidence](../docs/research/n01/EVIDENCE.md)).

## Existing Aste instruments set a demanding baseline

The artistic brief treats Vladislav Delay's sparse physical material, Photek's
precise rhythmic editing and Elektron's event-level performance ideas as
conceptual reference points. This research did not measure those artists'
methods, evaluate Elektron hardware, or reproduce proprietary algorithms.
The engineering comparison is against Aste's actual code and contracts at
baseline `8a603dcbc59b52a58637fe6305676efc5a406f80`.

| Capability | Existing family | What N-01 must demonstrate |
|---|---|---|
| Polymeter, probability, conditions, timing | Impulse: eight visible tracks, independent 1–32-step cycles, track timing and 1–4 ratchets | These alone justify no new instrument |
| Repeatable variation | Impulse: position-keyed seeded decisions | Retain reproducibility across buffers, transport and gesture replay |
| Individual sound settings | Impulse: track controls and Off/Hit/Accent cells | Per-event snapshots and repeat-specific pitch/timbre changes must improve composition |
| Capture, reverse, splice, pitch | Loop: three tape decks and generational transformation | Local selection plus master takeover must beat routing and editing effort |
| Temporal return | Products own distinct timing policies | Make Continue, Suspend/Resume and Rejoin predictable as one instrument |
| Session audio memory | Loop schema 3 retains captured generations | Do not mistake an event seed for a reproducible held audio fragment |

These distinctions follow the current
[Impulse specification](../docs/products/impulse/SPECIFICATION.md),
[Loop state format](../docs/products/loop/STATE_FORMAT.md) and
[architecture](../docs/ARCHITECTURE.md). Some older Loop architecture/parameter
text still describes schema 2; the current product state specification takes
precedence. No unrelated documentation migration is part of this experiment.

Three architectures compete. Extending Impulse is cheapest for event locks but
would burden an existing product and parameter contract with fragment/state
semantics. Composing Impulse → Loop preserves working instruments and is the
strongest immediate baseline, but does not establish shared phrase ownership
or independent local capture. A separate small engine makes time ownership
testable without changing either product; this is the selected **research**
boundary, not an accepted product decision. Reuse Aste's real-time audit,
measurement/WAV conventions and eventual UI entry/accessibility idioms. Do not
extract universal transport or link production processors merely because
similar terminology exists ([architecture](../docs/ARCHITECTURE.md),
[real-time rules](../docs/REALTIME_SAFETY.md)).

## Three modest sources expose temporal differences

The prototype's sources are deliberately narrower than its artistic ambition.
Pressure and Impact are damped sinusoidal combinations with a two-millisecond
attack; Dust mixes low-passed and high-passed seeded noise. These are not
validated physical models, novel synthesis claims or finalized sound identities
([source implementation](../research/n01/core.cpp)).

| Candidate | Implemented experiment | Alternative and selection question |
|---|---|---|
| Low pressure | Low sinusoid with a quiet inharmonic component; no obligatory kick sweep | Noise-excited low resonator: does less definite pitch improve material without wasting headroom? |
| Resonant impact | Two damped inharmonic oscillations with event-specific pitch, gain and decay | Few-mode resonator: does independently damped structure add useful articulation beyond Impulse Body/Metal? |
| Shaped noise | Deterministic filtered noise, finite decay and spectral mixture | Band-pass excitation: can it avoid familiar hat-like bursts while remaining controllable? |
| Microscopic fragment | Short selected captured audio replayed through the same bounded fragment reader | Overlapping grains would smooth/edit differently, but add scheduling and obscure attacks |
| Imported fragment | Not implemented; not needed to test time ownership | Later off-thread decoding, bounds checking and explicit session ownership; no browser or codec scope now |

Impulse already combines low oscillations, inharmonic components, filtered
noise and nonlinear loading, so timbral overlap is substantial. A source-only
comparison must hold event timing and loudness treatment constant; an
instrument comparison must also measure the work required to create and repeat
the composition. These answer different questions
([Impulse DSP](../apps/impulse/impulse_processor.cpp),
[listening protocol](../docs/testing/LISTENING.md)).

The event model caps three tracks at eight events each and eight repeats per
event. Each event owns pitch, decay, color, gain, probability, cycle condition,
microtiming, repeat interval, acceleration, gain decay and pitch/timbre change.
Intervals follow `I[k] = I0 × acceleration^k`; repeat trains cannot extend into
their own next cycle. A pure hash of seed and event identity controls acceptance
and bounded variation. Probability zero stays silent. This tests repetition
policy without building eight elaborate synthesizers
([lab-local types](../research/n01/core.hpp),
[validation and scheduler](../research/n01/core.cpp)).

## Host, phrase and fragment clocks have different owners

Let `H` be host quarter-note position, `P` phrase position, and `R` a fragment's
sample read index. Within a supplied constant-tempo segment,
`H(n) = H0 + n × BPM / (60 × sampleRate)`. Normally `P = H`; suspended duration
accumulates an offset so `P = H − suspendedBeats`. Each local cycle derives from
`P`, while captured playback advances in samples. Tempo changes therefore alter
musical scheduling, not the pitch or sample length of a frozen fragment. A
future beat-stretched capture mode would be a separate algorithm and contract.

**Continue preserves host phase; Suspend preserves phrase state.** Continue
renders source events, local readers and recording underneath master playback;
release reveals the phrase at its current position without replaying hidden
events. Suspend freezes all of that underlying state while the master readhead
continues. Release resumes the held phrase at the current tempo, retaining its
offset from the host. Explicit Rejoin immediately aligns `P` to `H` and clears
voices and capture history. The wireframe's next-boundary Rejoin is a proposal,
not implemented behavior. Rejoining automatically on release is a credible
alternative, but skips held material; accelerated catch-up is rejected from
minimal scope because it adds unpredictable event density
([implementation](../research/n01/core.cpp),
[interaction specification](../docs/research/n01/INTERACTION.md)).

| Stage | Ownership and precedence in the prototype |
|---|---|
| Transport boundary | Invalid/stopped transport produces silence; a detected seek resets source/fragment state before actions |
| Commands | At most 64 per callback; validated sample offsets and stable supplied order, applied before that sample's synthesis |
| Sound event | Finite source retriggers; exact onset ties choose the later event slot; distinct onsets within one sample coalesce |
| Local fragment | Records raw source, then replaces that voice's audible output; never recursively retriggers synthesis |
| Master fragment | Records the post-local mix, then replaces the mix; simultaneous local/master captures select preceding samples, not newly rendered current-sample audio |
| Structural void | Final-output gate overrides every audible layer; ordinary absent events instead allow existing tails |

Master replacement cannot feed back into its own capture. Capture banks swap
ownership rather than copying a large region at an action boundary; recording
history resets and must refill before another selection. A failed selection
does not invent unavailable samples. Master capture is before the final VOID
gate: it can contain underlying material inaudible during a void, but playback
still cannot fill the current protected void. Suspending inside a void preserves
that silence until release/rejoin. This is explicit current behavior to evaluate,
not a hidden “smart” exception ([fragment code](../research/n01/core.cpp)).

The scheduler assigns an onset to its containing half-open sample interval,
so placement can be less than one sample early. A ceiling convention would
instead be less than one sample late; neither should be confused with exact
continuous-time placement. Integer sample continuity and PPQ agreement within
half a sample detect discontinuity independently of callback size. Missing or
non-finite timing is rejected rather than converted to 120 BPM/beat zero.
The prototype accepts constant-tempo segments, not an explicit tempo-ramp map.
Steinberg documents that a host can leave an intra-block cycle jump unreported
until the next block, and JUCE timing fields are optional. Consequently no
offline test proves recovery of information Cubase has not supplied
([VST3 ProcessContext](https://steinbergmedia.github.io/vst3_doc/vstinterfaces/structSteinberg_1_1Vst_1_1ProcessContext.html),
[JUCE PositionInfo](https://docs.juce.com/master/classAudioPlayHead_1_1PositionInfo.html)).

## Bounded evidence comes before product commitments

The standalone lab builds without JUCE; processing is `noexcept`, with fixed
event/command bounds and capture allocation only during preparation. Three
250-ms mono histories and a 500-ms stereo master, each double-banked, consume
**672,000 audio bytes at 48 kHz and 2,688,000 at 192 kHz**, plus engine state.
That exceeds Aste's general 256-KiB budget: it is a disclosed laboratory tradeoff,
not an approved release exception. Proposed product goals are zero algorithmic
latency, zero callback allocations/locks/I/O and under 1% of one reference core
at 48 kHz/128 samples; three-megabyte total DSP memory is a candidate ceiling
requiring review. The measured CPU result, compiler, hardware, build and workload
belong in the [evidence record](../docs/research/n01/EVIDENCE.md), not an estimate
([existing budgets](../docs/PERFORMANCE_BUDGETS.md)).

| Experiment | Completed artifact or bounded status | What it does not establish |
|---|---|---|
| N1 deterministic scheduler | Analytic onset/condition tests, per-event settings, seeded scheduling | Arbitrary host-map accuracy |
| N2 repeat envelopes/acceleration | Geometric spacing, gain decay, pitch/color mutation; source sketches | Preferred articulation or alias quality |
| N3 local loops | Independent cycles; bounded historical capture/reverse/shorten/release tests | General granular synthesis |
| N4 master fragments | Stereo replacement, finite repeat/held playback, explicit modes | Session recall of captured history |
| N5 nested semantics | Continue-return, suspend offset, rejoin, partition and protected-void checks | Human predictability of these rules |
| N6 transport stress | Six sample rates, irregular blocks, zero frames, seeks, invalid timing, tempo-step cases and callback audits | Cubase, unseen tempo ramps or hidden jumps |
| N7 interface | Two inspected SVGs and interaction specification | Functional input, VoiceOver or automation |
| N8 family comparison | Source/contract duplication analysis; render measurements recorded separately | Comparative musical success or editing-effort advantage |

The table identifies implemented coverage, not a substitute for the actual run
results in [EVIDENCE.md](../docs/research/n01/EVIDENCE.md) and
[tests](../research/n01/tests.cpp). The unmodified core baseline passed 50 of
51 checks; dependency-security review expired on 2026-11-07. That failure remains
visible. A known timing fault was detected during scheduler testing; sanitizer,
negative-check and rendering outcomes must be read from their recorded runs.

Smooth fragment mode uses fixed 32-sample entry/release blending and edge
tapers; hard mode preserves deliberate cuts. Fixed samples mean different
durations at different rates. These simple fades are not proof of click-free
arbitrary transitions: very short selections lose amplitude, cuts/rejoin/VOID
edges can click, and overlapping/windowed alternatives require listening and
spectral measurement. No imported-file state, production serializer, gesture
recorder or stable automation mapping exists. A seed reproduces decisions, not
captured history; identical audio requires identical initial state, rate,
transport and accepted action history. A future versioned state must specify
whether audio is embedded or deliberately absent, never silently substitute
empty memory for a saved hold ([state precedent](../docs/products/loop/STATE_FORMAT.md)).

## A small interface must prove control, not complexity

The [default](../docs/research/n01/default.svg) and
[held-state](../docs/research/n01/held.svg) wireframes preserve Aste's near-black,
off-white technical layout and strong negative space. A dominant master timeline
shows protected voids; three rows show independent cycles and repeats; one
selected-item inspector supplies progressive detail. Host, phrase and fragment
positions remain distinct, with audible ownership and return mode always
visible. This avoids copying hardware pages and avoids hiding the consequence
of a master takeover ([family UI rules](../docs/UI_SYSTEM.md)).

The proposed interaction contract includes typed sample/beat values, fine
nudging, separate selection and audition, explicit Tab order with roving timeline
focus, accessible names/units/state, non-color feedback, and no global shortcut
that steals Cubase transport. The proposed 1024×680–1800×1060 range must keep
essential actions visible. Gesture capture records bounded semantic actions
and release edges rather than UI animation frames. VST3 automation supplies
sample-offset value queues; a future adapter must preserve those semantics
instead of claiming sample accuracy from block snapshots
([interaction details](../docs/research/n01/INTERACTION.md),
[VST3 parameter queues](https://steinbergmedia.github.io/vst3_doc/vstinterfaces/classSteinberg_1_1Vst_1_1IParamValueQueue.html)).

The minimal candidate remains three objects, event locks, one local fragment
per object, one master selection, Continue/Suspend, Release/Rejoin and explicit
silence. Reject eight expanded drum engines, recursive repeat multiplication,
automatic filling of voids, a sample browser, arbitrary routing, a tape-generation
clone, oversampling without measured need, and premature hundreds-of-parameter
host contracts.

The musical gate is a repeatable task trial against Impulse alone, Impulse →
Loop and ordinary DAW bounce/editing: make a sparse phrase, accelerate one event,
repeat a local transient under a master hold, preserve a void, then return at a
predicted position. Retain exact edits and compare completion effort, mistakes,
return predictions and repeatability. Listen blind at matched treatment using
the repository response-before-key protocol; record no preference honestly.
Repeat with keyboard-only and VoiceOver in Cubase. Advance only when a person
can repeatedly create a valued result with less effort or clearer control than
the existing family. If the distinction disappears under that comparison,
**stop N-01 as a product and retain the useful research**. No completed human
trial currently establishes that gate.
