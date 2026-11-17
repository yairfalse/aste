# Research, decisions and proposed instrument

## Repository reconnaissance and baseline

The working repository already contains eight 0.8.0 development instruments and
substantial uncommitted personal work. This experiment neither resets that work
nor changes those instruments. Reviewed: `docs/ARCHITECTURE.md`,
`docs/REALTIME_SAFETY.md`, `docs/PARAMETER_MODEL.md`, `docs/UI_SYSTEM.md`,
`docs/TESTING.md`, the artistic brief, Loop’s processor and take storage,
Field’s processor and nested-memory ownership, N-01/R-01 lab builds and tests,
and the family quality/measurement helpers.

Loop explicitly records new decks and retains take history. Field’s diffuse
feedback-delay network, pitch returns and local immutable phrase capture serve
stored spatial energy. Neither is an arbitrary network of separately captured
grain outputs. G-01 therefore owns a new small core. It reuses the **offline**
WAV/loudness/fixture helpers and the test-only macOS OS-call audit. It does not
extract a universal granular engine from Field or make products depend on one
another. Existing Loop/Field cores appear only in the comparison executable.

## Relevant DSP research

Csound’s [syncgrain documentation](https://csound.com/docs/manual/syncgrain.html)
distinguishes in-grain read increment from the source pointer movement between
grains and identifies overlap count as a CPU concern. This supports separating
our source cursor speed from pitch increment. Its existence also means that
independent granular time alone is **not** G-01’s differentiator.

[Partikkel’s documented per-grain model](https://csound.com/docs/manual/partikkel.html)
includes timing distribution, envelope and spatial controls. It is a useful
reference for controllable clouds; no Csound implementation is copied here.
G-01’s hypothesis is instead recursive capture across independent memories and
musically legible routing.

Julius O. Smith’s [windowed-sinc discussion](https://www.dsprelated.com/freebooks/pasp/Windowed_Sinc_Interpolation.html)
explains rate-dependent low-pass interpolation. We compare a convex linear
reader with a short windowed-sinc reader whose cutoff decreases with read rate.
The finite filter is an approximation, not an alias-free guarantee.

| Alternative | Benefit | Problem / cycle decision |
|---|---|---|
| Mutable circular source ring | Immediate rolling memory, modest storage | Overwrites can alter active grains; safe multi-generation ownership gets opaque. Not selected initially |
| Copy every grain into its own buffer | Fully independent grain life | Long grains × high rates × voices consume excessive memory/copy spikes. Rejected |
| Three immutable capture banks per node | Readers keep exact source data; O(1) publication and clear | Long grains can pin banks and stall new capture. Implemented with visible counters |
| SCC/topologically sorted immediate feed-forward paths | Lower cascade delay | Edge semantics change when a cycle closes; harder scene transitions. Deferred |
| Same fixed sample delay on every edge | Order-independent cycles and sample-exact partition equivalence | Even feed-forward paths have one-sample edge delay; capture windows add much more latency. Selected |
| Fixed-phase overlap-add | Cheap independent pointer motion and explicit fragments | Tonal sidebands, transient repetition and coloration at extreme stretch. Implemented |
| WSOLA / transient-aware alignment | Potentially cleaner time modification | Correlation/search cost and deterministic bounds; could reduce desired fracture. Proposed comparison, not implemented |
| Phase vocoder / phase-locked spectral stretch | Slow sustained spectral material | Latency, transient smearing and different identity. Research alternative only |
| More nodes / modulation matrix | More combinations | No evidence three are insufficient. Rejected expansion |

## Explicit routing and time semantics

At sample **t**, all three nodes capture `live[i] * input[t]` plus a weighted
sum of **previous[t-1][j]**. Only after every node has rendered is the previous
output array replaced. This applies to A→B, B→A and self-routing alike; there
are no immediate edges and no algebraic feedback solution. Source outputs are
pre-monitor-level, so lowering a node’s audible output does not silently remove
its feed into another memory. This must remain visible in a product.

Route levels range 0–1.5. Each destination’s smoothed nonnegative incoming route
row is proportionally reduced when its sum exceeds 0.995. Live input is separate.
The matrix retains requested values; telemetry exposes effective row scaling.
This is intentional gain management, not a claim each requested edge remains
independent above the ceiling. Route/live levels smooth over 15 ms; node monitor,
master and mix smooth over 10 ms. Matrix morph interpolates routing/live levels
only, not hidden sound presets. The `morph` helper preserves all non-routing values from the starting state.
The lab performs Time/Erosion/Spread explicitly, while preserving its chosen
node sounds. There is no halfway switch of sound parameters.

Each node uses a sample-clocked scheduler (0–240 base grains/s, maximum one
scheduler decision per sample, bounded cluster of up to three births). No free
grain slot means the new birth is dropped, counted and never queued indefinitely.
There are 48 grains per node. Time scattering perturbs the next scheduler
threshold, separately from pan scattering. The scheduler is deterministic, not
a Poisson-process claim. Per-node PRNGs are seeded during prepare/reset; changing
the seed in a running control snapshot does not reseed history. A product needs
an explicit reseed action.

Free nodes advance regardless of DAW playback. Synced nodes use BPM/division;
stopping transport stops births while existing grains finish. Capture policy
continues: frozen memory stays, but unfrozen capture can replace material even
while stopped.
Seeks/start-stop are detected from PPQ continuity and reset only synced birth
phase. Invalid PPQ disables host synchronization for that call. Events and
control snapshots carry sample offsets, so commands and automation may fall
inside any host block. Batches are capped at 128 entries each; unsorted/out-of-
bounds command batches are rejected before commands execute. Core ownership is
single audio thread; no claim is made that a JUCE/VST3 boundary already delivers
sample-offset automation to this lab.

## Memory and inheritance

Each node has three stereo banks allocated in prepare, a published bank, a
recording bank and up to one additional reader-pinned bank. A full recording
publishes only when a bank with zero reader references is available. Existing
grains retain their bank; a referenced bank is never overwritten. If none is
free, capture stalls with its complete recording retained until a reader exits.
No buffer allocation or bulk erase occurs in processing. Clear invalidates the
selected node’s lengths, references and grains; a 3 ms outgoing transition is
allowed. `reset` clears the whole network and outgoing history. A node clear is
not a global panic: other memories may send material back into it.

Capture length is 20 ms–4 s, capped by prepared memory. Freeze or capture-off
publishes a partial recording before stopping writes, so a short fragment need
not vanish merely because the nominal window was not full. Published memory is
played cyclically within grain boundaries. Very short banks can therefore make
long grains periodically repeat. Source-bank wrap is not a splice editor and
can add a boundary discontinuity; windows do not make arbitrary interior wraps
transparent. This is an unresolved quality issue.

Accumulation mixes a bounded fraction of the previous published bank into new
capture before saturation; replacement is its zero endpoint. This acts inside
each node, independently of graph recursion. Erosion low-pass filters and
nonlinearly softens individual grains; new captures inherit those changes.
Inheritance currently reduces new-grain pitch and position mutation as it
approaches one. It is an identity-scatter hypothesis, not similarity analysis
or an exact measure of recognizable source character. Repeated source/target
experiments must decide whether that macro is musically meaningful.

## Speed, windows and stereo

Independent mode moves the **source selection cursor** at Speed but reads each
grain at its pitch increment. Varispeed multiplies the in-grain increment by
Speed. Reverse reverses both cursor travel and read direction. Speed ranges
0–8, pitch ±24 semitones, grain size 1–2000 ms. A Speed of zero in independent
mode holds selection; in varispeed it also stops the sample read. Long/slow
textures still use discrete overlapping grains, not phase-locked stretching.

Hann, triangle and intentionally short-edge fragment windows are implemented.
Overlap gain is divided by `max(1,sum(window))`, preventing overlap count from
multiplying amplitude. This can pump at sparse boundaries; it is not equal-power
cloud compensation. Linear interpolation is cheap but aliases when pitched up.
Sinc16 uses 64 fractional phases and six conservative octave cutoff tables.
Its finite kernel can overshoot; downstream clamps bound it. There is no
anti-alias guarantee for nonlinear erosion, window sidebands or every pitch.

Stereo channels remain paired. Width attenuates source side; per-grain balance,
scatter and drift never introduce polarity inversion. Therefore width does not
rely on phase cancellation. It can still reduce mono amplitude and inherited
stereo phase differences remain. Density does not automatically widen the cloud.

## Stability, memory and performance

Capture uses bounded soft saturation, grains are overlap-normalized and node
output is clamped to ±2. Wet summation uses tanh. Final Master (0–1) acts after dry/wet mixing.
Finite dry input is capped at ±8, and final mix is clamped to ±8. These are sample
bounds, **not a proof of BIBO decay or guaranteed silence after arbitrary cyclic
history**. Sinc overshoot and accumulation may sustain a bounded system. Silent
initialization generates exact silence; explicit reset is the way to discard
all energy. NaN/Inf input/control sanitation and row normalization are counted.

Default four-second memories use approximately 13.23 MiB at 48 kHz and
52.78 MiB at 192 kHz, plus fixed state included in measured byte counts. Prepare
may allocate; process/clear/reset do not allocate/free, lock, wait, log or do I/O.
The tests audit new/delete and selected OS calls, not every conceivable API.

Benchmarks measure complete process calls at up to 144 active grains. Linear
interpolation has usable measured headroom on this laptop; Sinc16 approaches or
exceeds deadlines at 192 kHz. See the exact CSV, including outliers. A product
must set a measured voice/quality budget per rate and expose exhaustion, rather
than promising these extremes are all simultaneously realtime. Grain-pool and
capture-bank pressure are separate counters. No optimization or extra nodes
will be added merely to conceal a failed timing budget.

## Difference from the Aste family

- **Loop:** manual captured takes, print and compare histories; source identity
  changes across recorded editions. G-01 has independent node memories and many
  concurrent fragments; a graph can revisit earlier material through cycles.
- **Field:** diffuse spatial energy in a delay network with pitch/grain returns.
  G-01 stores addressable source banks and launches explicit grain voices. Its
  timing and sound do not require a reverberation network.
- **Resonator:** energy excites tuned modes; G-01’s fragments retain sampled
  waveform material. No modal bank, chord generator or resonant-memory engine
  is used to make the granular network sound impressive.

Actual Loop and Field comparison WAVs use current 0.8.0 cores and matched
loudness pairs. A conventional single-node granular baseline uses the same G-01
grain engine with all routes disabled; this controls for engine/window quality,
not a comparison to a commercial granular effect.

## Minimum candidate and next decision

Keep three nodes, nine directed routes plus live sends, one selected node
inspector, visible freeze/clear, route scenes, a final master and four candidate
macros (Time/Spread/Erosion/Inheritance). No plugin identity, public parameter
contract, installer entry or family version bump is introduced. Internal
`G01LAB 1` configuration text is explicit and transactional; it saves controls,
not current grains or capture audio. UI-study JSON is a separate prototype
format and is clearly labelled as such.

Before product integration: musician auditions the matched cascade pair, slow
textures and performed three-minute run; decides whether bank stalls and
fragment looping are musical or obstructive; compares a transparent/stretch
alternative; and approves a small macro mapping. Then freeze parameter IDs,
choose audio-memory recall policy and a measured CPU budget, and build the thin
JUCE boundary. G9 Cubase/VST3 validation remains unperformed; offline transport,
automation and control-recall simulation is evidence only for the core.
