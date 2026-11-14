# Density and Harmonic musical power

Source baseline: `8a603dc`, inspected 2026-11-14. This review reads current
production and documented history; it does not claim a new audio render,
listener judgment or Cubase session. Historical measurements below are explicitly
historical. Current experiments are owned by the audit laboratory.

## What limits Density after its earlier character revisions?

### Takeaway

The current compressor is not the weak-threshold, half-dry version criticized
in earlier feedback. Its likely remaining limitations are interacting macro
laws, source-dependent timing, and limited explanation of internal operating
point—not a demonstrated lack of numerical compression or available drive.

### Cited Findings

- Current Density maps 0–100% to threshold −18…−46 dB, ratio 3…60,
  saturation drive 1…9, release curvature 1…5 and makeup +12…+22 dB.
  Crush changes threshold by `12*(1-crush)`, ratio by `1+crush*(ratio-1)`,
  makeup by multiplication, and saturation participation. The default
  Density=.5 / Crush=.65 therefore implies threshold −27.8 dB, ratio 20.825,
  makeup 11.05 dB and saturation drive about 3.25035 at Drive=0. These are
  evaluations of source formulas, **not measured audio results**.
  — [mapping, lines 298–315](../../apps/density/density_processor.cpp#L298);
  [gain computer, lines 504–542](../../apps/density/density_processor.cpp#L504).
- Actual order is input Drive → detector/compression → makeup → normalized
  tanh → fixed crush clip → dry/wet blend → Output → optional output clip.
  Positive Drive also raises saturation drive by up to 75%. Thus there are
  two clipping locations and Output is not always a harmless linear trim:
  changing it can engage Protection. Crush zero removes compression and
  almost all tanh participation, but **does not bypass the fixed crush clip**.
  — [transfer functions, lines 53–73](../../apps/density/density_processor.cpp#L53);
  [complete graph, lines 482–581](../../apps/density/density_processor.cpp#L482).
- Default is Blend 90%, Output −7 dB, attack 1 ms, release 180 ms, detector
  HPF 90 Hz, stereo link 100%. Coloured factory starting points retain
  50–80% wet proportions, not universally fully wet.
  — [defaults](../../apps/density/density_processor.hpp#L8);
  [factory presets, lines 30–46](../../apps/density/density_plugin.cpp#L30).
- The detector sees driven, high-passed audio, not the unfiltered audible
  signal. Stereo link interpolates toward the larger channel's detector;
  release duration is multiplied by `1+(releaseCurve-1)*clamp(envelope,0,1)`.
  Therefore release shape depends on absolute source level and Drive as well
  as duration and Density; it is not an independent programme-history model.
  — [detector and envelope, lines 490–538](../../apps/density/density_processor.cpp#L490).
- Drive/Blend/Output use cascaded 3+3 ms smoothers, Density/Crush/Stereo 10 ms,
  Attack 5 ms in log time; Release and detector HPF are recomputed per block.
  These preserve useful stable automation, but changing every block is not the
  same claim as sample-offset host automation. The adapter reads one parameter
  snapshot per process call.
  — [prepare, lines 342–365](../../apps/density/density_processor.cpp#L342);
  [process coefficients, lines 470–479](../../apps/density/density_processor.cpp#L470);
  [adapter](../../apps/density/density_plugin.cpp#L448).
- The UI already has explicit first-action text (“Drive pushes / Density
  shapes / Crush intensifies / Blend mixes”), typed/fine/reset interaction,
  input/output levels, gain reduction, presets and a visible Protection switch.
  Its GR bar saturates visually at 30 dB although the numeric reading continues;
  the display reports maximum detector reduction, not the effective parallel
  output's dynamic reduction or saturation activity.
  — [meter, lines 200–255](../../apps/density/density_plugin.cpp#L200);
  [editor layout, lines 264–355](../../apps/density/density_plugin.cpp#L264).
- The original track-level failure was real: ADR 0018 records the previous
  −11.8 dB default effective threshold giving no compression on −24/−18 dBFS
  peak tones. Its revised mapping measured 3.37/9.09 dB reduction. ADR 0019
  then changed defaults to 90% Blend/−7 dB, recording a 0.365 dB output
  response to a 6 dB input-level increase. These past fixes must not be
  presented as newly discovered or as evidence of current musical preference.
  — [ADR 0018](../../docs/adr/0018-track-level-impact.md);
  [ADR 0019](../../docs/adr/0019-tone-shelves-and-direct-blend.md).

### Inferences

**What works structurally:** independent dry and processed paths, wide input
drive, fast/slow articulation settings, source-selective detector, continuous
stereo linking and repeatable automation already permit substantially different
processing mechanisms. A blanket “not enough compression” diagnosis is not
supported by the current mapping.

**Hypothesis D1 — control interactions reduce deliberate control (high source
confidence, musical impact unmeasured).** Density and Crush both alter several
of the same dimensions. Raising Density lowers the post-compression level while
raising makeup and saturation drive; raising input Drive increases detector
reduction as well as excitation. In an above-threshold steady state, the ideal
gain computer gives post-compression amplitude proportional to input amplitude
to power `1/ratio`. At ratio 60, +24 dB pre-compressor Drive contributes only
about +0.4 dB to that steady amplitude before its additional saturation law.
That is a derivation, not proof that the full dynamic graph sounds unchanged.
The practical risk is that high values differ mostly in recovery or transients
while panel wording suggests independent dimensions of force.

Minimal experiment: sweep Density/Crush/Drive independently and pairwise across
seven settings on percussion, bass, dense material and sustained tones. Retain
raw gain, crest, transient/body ratio and GR; then externally level-match and
measure residuals and spectral change. Compare rank/correlation of sensitivity
vectors, not just endpoint loudness. Negative controls: Blend=0 with unity
Output; silence; Crush=0 at low enough input to avoid the fixed clip. Include
Crush=0/high Drive as a separately named clip-participation case, not a neutral
control. Do not change Output to match variants if Protection can engage.

**Hypothesis D2 — envelope law restrains groove (medium confidence).** The
single peak follower plus absolute-level-dependent release can favor one source
calibration and keep dense material suppressed. The existing programme-memory
or RMS/peak detector experiments are better starting points than a seventh
detector. Compare equal sustained reduction, isolated-event reset, body recovery,
and full stereo graph at several input levels. A useful alternative must improve
articulation on repeated musical phrases, not merely show less GR.

**Hypothesis D3 — aggressive settings expose undesirable aliasing before a
useful new character (known mechanism, preference unknown).** Normalized tanh
and fixed clipping at 1× are not transparent at high-frequency drive. The
existing 73/33 halfband candidate can be re-rendered at the **current** mapping,
latency aligned and externally level matched. This challenges a provisional CPU
choice, not callback safety. More distortion is not automatically an improvement.

Across the requested dimensions: sonic range and dynamic action are present in
code; character and usefulness are unproven. Responsiveness/depth require
level-matched sensitivity tests. Interaction is strong but partly overlapping.
Control can improve through source-level and effective-operation feedback rather
than extra parameters. Performance manipulation is currently ordinary host
automation, not an instrument-specific gesture model. UI hierarchy is coherent
but hides threshold/ratio/release consequences. Workflow has small preset sets,
portable scalar state and explicit Output, but no gain-linked comparison action
or completed target-host usability evidence.

### Gaps

No fresh Density renders, preference results or GUI screenshots were generated
by this subreview. Current whole-graph alias spectra, relative transient/body
changes and parameter-sensitivity degeneracy remain to be measured by the audit.
The creator's feedback is substantive but does not identify exact installed
binary/version, source level, settings or failed musical gesture; the report
must not assume the current checkout is the installed bundle.

## Where is Harmonic's nonlinear identity available or hidden?

### Takeaway

Harmonic is a real broad EQ, but its nonlinear identity is conditional: positive
gain, sufficient input energy near the nonlinear band's center, and favorable
serial interaction. A neutral default and low-level sources can make its hero
macro uneventful without any disconnected parameter. The cheapest useful test
is operating-point calibration using existing Input/Output controls.

### Cited Findings

- Four serial sections run Foundation→Body→Presence→Air. Foundation/Air are
  low/high shelves with slope S=1; Body/Presence are bells. Positive bell Q
  broadens as gain rises; cuts tighten. Shelf labels denote plateau gain and
  half-gain corner frequency, not peak-center gain.
  — [contour, lines 63–109](../../apps/harmonic/harmonic_processor.cpp#L63);
  [Q, lines 169–172](../../apps/harmonic/harmonic_processor.cpp#L169);
  [serial processing, lines 290–308](../../apps/harmonic/harmonic_processor.cpp#L290).
- Every band additionally owns a **fixed-Q 0.9 bandpass** linear/nonlinear
  pair at its frequency. The nonlinear integrator blends toward
  `tanh((1+5h)*linearBand)/(1+5h)`. Output adds
  `1.1*h*sqrt(max(gain,0)/12)*(coloured-reference)` to the linear contour.
  The outer nonlinear sections remain band-state corrections despite their
  linear contours becoming shelves.
  — [nonlinear state update, lines 125–162](../../apps/harmonic/harmonic_processor.cpp#L125);
  [band integration, lines 175–204](../../apps/harmonic/harmonic_processor.cpp#L175).
- All default gains are zero, Harmonic is 35%, Input/Output unity. Therefore
  Harmonic 0–100 is exactly inactive on the initial zero-gain signal path;
  settled cuts also have zero correction. This is explicitly explained in the
  panel footer, not wholly undiscoverable.
  — [defaults](../../apps/harmonic/harmonic_processor.hpp#L8);
  [panel, lines 319–325](../../apps/harmonic/harmonic_plugin.cpp#L319).
- Input/Output each span ±18 dB; band gains ±12 dB. Input is before every
  band and Output after the whole graph. Output cleanup bounds at ±16, **not
  ±1**, and there is no user-facing protection limiter. Four serial boosts
  can therefore consume substantial downstream headroom while remaining finite.
  — [ranges](../../apps/harmonic/harmonic_plugin.cpp#L376);
  [cleanup](../../apps/harmonic/harmonic_processor.cpp#L23);
  [gain location](../../apps/harmonic/harmonic_processor.cpp#L294).
- The activity meter takes the maximum absolute nonlinear correction across
  bands/channels/samples, then displays `clamp(activity*20,0,1)` with display
  decay. It is actual DSP-derived state, but it is not a harmonic-distortion
  ratio, loudness estimate, or measurement of novel spectral content: a
  correction also contains fundamental attenuation/phase change.
  — [activity accumulation](../../apps/harmonic/harmonic_processor.cpp#L195);
  [meter rendering](../../apps/harmonic/harmonic_plugin.cpp#L194).
- The production report currently runs only two one-second four-sine fixtures
  (“broad”, “sculpt”) at six rates. Its pass gate is finite output, activity>0,
  latency0 and peak≤16. That is useful safety evidence but cannot establish
  distinctiveness, useful extremes, or broad musical range.
  — [report, lines 761–826](../../apps/dsp-lab/harmonic.cpp#L761).
- Six factory settings include neutral Default, Foundation, Continuum,
  Presence, Hard Air and Negative Space. Current presets already include
  6–8 dB boosts and trims. Ordinary automatable IDs/schema1 are stable, but
  earlier revisions intentionally changed the render meaning of stored values.
  — [presets](../../apps/harmonic/harmonic_plugin.cpp#L39);
  [ADR 0019 compatibility](../../docs/adr/0019-tone-shelves-and-direct-blend.md#compatibility).

### Inferences

**What works structurally:** proper low/high shelves plus broad bells support
deliberate contouring; exact neutral and cleaner cuts provide trustworthy
anchors. Input can alter the nonlinear operating point independently of the
final level. Serial order gives real interaction: a lower band's output drives
all later nonlinear sections.

**Hypothesis H1 — nonlinear operating-point mismatch (high mathematical
confidence, musical benefit unknown).** Around zero, tanh's difference from
linear is cubic; relative distortion therefore falls approximately with input
amplitude squared. Roughly 20 dB lower input can mean 40 dB lower relative
third-order contribution in the small-signal regime. Historical canonical
tests at 0.5 peak do not establish meaningful character at −36/−24 dBFS.

First compare the unchanged graph at Input/Output 0/0 against +12/−12 and
+18/−18, then externally match remaining level differences. Sweep −42, −36,
−24, −12 and −6 dBFS inputs, individual +3/+6/+12 boosts, two-band and
four-band cases, tones/IMD pairs and varied music. If existing gain staging
reveals useful character, a linked gain gesture or calibrated input cue is
smaller and safer than a new engine.

A stronger isolated candidate can apply fixed predrive and reciprocal scaling
**only to the colored/reference residual branch**, preserving the ordinary
contour. Keep the current graph as an exact baseline; no stable parameter or
production-law change. Negative controls must include flat gains, cuts only,
Harmonic=0, empty input, and high-level overload recovery. Judge nonlinear
change separately from louder EQ; measure IMD, folded content and changed
fundamental gain, not H3 alone. Reject if low-level audibility simply buys
high-level harshness or destroys contour meaning.

**Hypothesis H2 — shelf/character mismatch (medium confidence).** The bass
shelf acts throughout its lower plateau, but its nonlinear branch remains
centered on the corner. Likewise Air's HF plateau is not identical to the
bandpass region. Compare equal boosts and equal source energy at the corner,
well inside the shelf and at an unrelated region; distinguish correct linear
shelf gain from weak nonlinear participation. If musically important, a
shelf-region excitation residual is a focused architecture candidate. It needs
fresh phase/neutral/high-rate references; do not blindly reuse the originally
rejected residual-clipping topology.

**Hypothesis H3 — serial order obscures control (medium confidence).** Early
boosts can increase later nonlinear drive; early cuts can suppress it. Test
individual bands versus combinations and compare order-sensitive deltas after
matching the linear contour. A parallel residual experiment could clarify
ownership, but has greater recombination/phase risk and should not precede H1.

Across the requested dimensions: sonic range is credible for EQ, conditional
for coloration; unique character is unproven. Responsiveness has exact dead
regions by design, plus likely low-level nonlinear insensitivity. Depth and
interaction need multiband, multilevel tests rather than activity>0. Control
would improve by showing engaged bands and working level near the hero control,
not by adding a spectrum animation. Performance is ordinary continuous host
automation with 20 ms filter/participation smoothing, 10 ms macro and 5 ms gain.
The UI already explains dependencies, but a prominent live inactive/low-drive
state could make them actionable. Gain management and fair bypass comparison
are the principal workflow hypotheses; real Cubase automation and native
accessibility remain unproven.

### Gaps

No objective rule establishes what amount of third harmonic is musically
meaningful on mixed material. No new listener preference, full production
multiband differential-alias matrix, or input-calibrated trial was performed in
this subreview. A static nonlinearity gain increase should not be called an
improvement before those comparisons. Current panel screenshots belong to the
separate interface audit; this source-based review does not substitute for them.

## Which historical choices should be reused or challenged?

### Takeaway

Both products have accumulated substantial experiments without completed
musical selection. Preserve their falsifiable references, but do not mistake
the chosen beta topology, a 1% CPU target, zero latency, or broad boost/clean-cut
preference for immutable safety requirements. Conversely, state compatibility
and callback safety cannot be traded away for a more impressive audition.

### Cited Findings

- Density already has six detector candidates: peak, RMS/peak, dual-time,
  programme memory, hybrid feed-forward and bounded feedback-inspired. Earlier
  event tests had insufficient recovery gaps and were corrected; later
  threshold changes required feedback recalibration and reversed a dual-tail
  comparison. Thus historical ordering is operating-point-dependent, not a
  universal verdict. No documented blind selection completes that research.
  — [Cycles 4–10](../../docs/products/density/CYCLE_10.md);
  [corrected isolation](../../docs/products/density/CYCLE_07.md);
  [later recalibration](../../docs/adr/0018-track-level-impact.md).
- Density's retained 73/33 oversampling integration added 44 samples and
  historically measured about 1.042–1.043% versus a 1% default target. It was
  held back for that budget, not callback allocation or instability. A blind
  pack exists, explicitly without recorded preference. Aliasing tables in
  earlier cycles use older drive/mapping conditions and are not current
  full-graph results.
  — [research history](../../docs/products/density/DSP_RESEARCH.md);
  [whole-instance measurement](../../docs/products/density/CYCLE_30.md);
  [blind-pack gap](../../docs/products/density/CYCLE_32.md).
- Harmonic residual excitation was rejected because increasing its macro
  collapsed a nominal +12 dB contour toward +2.16 dB. Pre-emphasis and bounded
  state-variable candidates survived; the latter had lower historical IMD/
  folded proxies and became internal beta without listening selection.
  Stronger production drive/participation and later broad contours/shelves
  supersede those original candidate measurements.
  — [candidate evidence](../../docs/products/harmonic/DSP_RESEARCH.md);
  [promotion decision](../../docs/adr/0007-harmonic-internal-beta-topology.md);
  [stronger mapping](../../docs/products/harmonic/CYCLE_06.md).
- Current constraints require finite bounded state, validated parameter/state
  input, no callback allocations/frees/locks/I/O/logging, explicit ownership,
  and correct latency reporting. CPU ceilings and initial zero-latency policy
  are documented product targets; they are not mathematical safety laws.
  — [realtime policy](../../docs/REALTIME_SAFETY.md);
  [budgets](../../docs/PERFORMANCE_BUDGETS.md);
  [quality decision](../../docs/adr/0002-density-quality-latency.md).
- Earlier revisions preserved identifiers/schema but intentionally changed
  existing render behavior. This audit's user explicitly requires existing
  sessions preserved absent approved migration; identical IDs alone are not
  enough. Keep alternatives laboratory-only and require explicit versioned
  behavior/opt-in design before production integration.
  — [historical compatibility](../../docs/adr/0018-track-level-impact.md#compatibility);
  [current shelf compatibility](../../docs/adr/0019-tone-shelves-and-direct-blend.md#compatibility).

### Inferences

Priority among these two products: H1 operating-point calibration is the
smallest high-value investigation, with an unchanged-graph control available.
D1 sensitivity/cancellation mapping is the first Density diagnostic; only then
choose an existing detector or oversampling finalist. H2 shelf-region character
and H3 parallel-residual ownership are deeper research, not automatic additions.
Preserve rejected audio/candidates and record why a result failed. Keep musical
impact, confidence, effort and compatibility risk as separate backlog columns.

Acceptance should ask whether a musician can predict a gesture, deliberately
reach contrasting outcomes, repeat those outcomes, and prefer or reliably
distinguish the result across several source classes after level matching.
Neither greater measured distortion, less aliasing, stronger GR nor a larger
parameter delta alone proves musical power. A finding that existing controls
already suffice is valuable and should lead to interaction/preset work rather
than an unnecessary architecture replacement.

### Gaps

Historical documents repeatedly leave real projects, matched listening,
supported-hardware budgets and native-host interaction open. This audit must
not convert those omissions into presumed failures—or presume success because
automated checks passed. The actual installed version and concrete failed
production gestures remain unknown from the supplied feedback.
