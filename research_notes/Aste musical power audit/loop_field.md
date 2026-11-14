# Loop and Field musical power

Repository-source audit at `8a603dcbc59b52a58637fe6305676efc5a406f80`.
The creator's report of insufficient power is substantive user evidence, not
a listening test performed by this reviewer. Numerical expressions below are
derived from source; new rendered measurements belong to the audit laboratory.

## Does Loop expose meaningful generational transformation?

### Takeaway

Loop has a genuinely cumulative mechanism, but its gain laws, capture balance
and timing semantics make it harder to predict than its simple labels imply.
There is also a concrete clock defect: the supposedly slow motion sources
restart every second rather than completing their intended cycles.

### Cited Findings

- Three decks preserve current and two older generations; RELOOP prints actual
  transformed playback, and printing from an older generation discards its
  newer branch. Print takes one loop duration, with no retrigger while busy.
  This is actual generational processing, not a cosmetic degradation knob.
  [Core](../../apps/loop/loop_processor.cpp), lines 235–285 and 377–395;
  [decision](../../docs/adr/0011-loop-generational-tape.md).
- At first capture, `existing=0`, and storage receives
  `input*(1−overdub)`. Default Overdub=0.5 therefore records at −6.0206 dB
  relative to input before the −3 dB default output or playback loading.
  At Overdub=1, an empty tape captures zeros. Feedback affects existing samples
  only while capture is active; it does not create continuously evolving
  feedback when playback alone runs. These are source-law deductions, not a
  newly measured audio result. [Core](../../apps/loop/loop_processor.cpp),
  lines 303–305 and 352–376; [defaults](../../apps/loop/loop_processor.hpp),
  lines 10–30.
- Record drives two different laws. Reproduce output is
  `tanh((1+4R)*x)/(1+4R)`, unity small-signal slope but lower asymptotic
  headroom as R increases. The print path is
  `tanh((1+5R)*x)/tanh(1+5R)`, whose small-signal gain increases with R.
  Thus a stronger Record setting can lower monitored peaks while making new
  generations hotter. Record is not merely a stronger static saturation
  amount. [Core](../../apps/loop/loop_processor.cpp), lines 271–285 and 371–382.
- Loss already acts twice: reproduce resolution reduction, then print
  bandwidth, resolution and up to 11% level loss per generation. Tape speed
  changes only print bandwidth. Repeated printing can change transient shape,
  spectral balance and gain even when transport controls are unchanged.
  [Core](../../apps/loop/loop_processor.cpp), lines 271–285 and 374–382.
- `modulationPhase_` advances by `1/Fs` and wraps at one. Wow, flutter and drift
  then evaluate noninteger multiples 0.47, 6.71 and 0.173 of that phase.
  Consequently all sources repeat a one-second fragment, not full 0.47/6.71/
  0.173 Hz cycles. Drift's forcing `sin(2π*0.173*phase)` is nonnegative on this
  interval and has a reset discontinuity; the one-pole drift smoothing uses
  fixed 0.99997/0.00003 coefficients, so its physical time constant varies
  with sample rate. [Core](../../apps/loop/loop_processor.cpp), lines 412–427.
- Pitch heads use an additive read offset with phase increment
  `abs(pitchRatio−1)/1024`, independent of signed varispeed. Away from windows
  and splice boundaries, the head read slope is approximately
  `signedSpeed + (pitchRatio−1)` when the grain is 1024 samples, rather than
  `signedSpeed*pitchRatio`. Speed/reverse/Pitch combinations therefore need
  measured pitch and transient tests before promising independent semitone
  transposition. At reverse 1× and +12 semitones, the within-window slope can
  approach zero; window changes still create sound. This is a causal
  hypothesis for confusing combinations, not a measured spectral verdict.
  [Core](../../apps/loop/loop_processor.cpp), lines 190–225 and 428–436.
- The pitch window is a fixed maximum of 1024 samples, so its duration changes
  with sample rate. Start, pitch, splice and duration are direct edit controls;
  Mix, Output, Speed, Record and Loss use 5 ms smoothing. Intentional edits and
  inevitable crossfade smearing must be evaluated on transients, not equated
  automatically with faults. [Core](../../apps/loop/loop_processor.cpp),
  lines 62–63, 190–225, 294–334;
  [parameter contract](../../docs/products/loop/PARAMETERS.md).
- Host Sync selects duration from BPM, not a host-PPQ phase lock or automatic
  bar-quantized capture. MIDI C gates capture, D triggers RELOOP, B/C♯ navigate
  previous/next generations at event offsets. GUI actions arrive through
  bounded atomic requests at callback boundaries, not host automation
  parameters. The user can sequence these actions through MIDI, but recording
  a mouse RELOOP performance is not equivalent to recording an automation
  lane. [Adapter](../../apps/loop/loop_plugin.cpp), lines 490–520 and 592–639;
  [parameter contract](../../docs/products/loop/PARAMETERS.md).
- Current schema **3** saves recorded tapes and generations, resampling on
  rate changes and restarting playback from tape beginning. Unfinished print
  destinations are not retained. Earlier architecture text and ADR 0011 say
  captured audio is not saved; those statements are superseded, not current
  defects. Snapshot memory is approximately 88 MiB at 48 kHz and increases
  with rate; off-thread restore can temporarily need a second engine plus
  decoded state. [State contract](../../docs/products/loop/STATE_FORMAT.md);
  [ADR 0016](../../docs/adr/0016-playable-defaults-and-tape-recall.md);
  [adapter](../../apps/loop/loop_plugin.cpp), lines 739 onward.

### Inferences

- **Highest-confidence experiment:** keep all signal laws and nominal motion
  depths unchanged, replace the common prematurely wrapped phase with bounded
  independent phases, and compare modulation spectra, reset discontinuities,
  six rates and long rendered passages. Keep production and candidate side by
  side; this changes existing rendered audio and is not session-transparent.
- **Small no-DSP improvement:** explain Overdub's endpoints as New↔Existing,
  and show that Feedback applies only during capture. A person turning
  “Overdub” fully up to add more new audio currently gets the opposite.
  Explain “tempo-derived duration” rather than implying full transport lock.
- **Separate experiment:** compare capture unity gain on empty tape only,
  without changing overdub semantics once audio exists. Measure and listen at
  matched output, retaining current first-pass attenuation as the baseline.
  This is not automatically an improvement: default headroom is a design
  preference and existing recordings made under it must not be reinterpreted.
- **Fundamental research:** determine whether Record should coordinate
  reproducible input loading and generational energy rather than two opposing
  gain conventions. Compare multiple generations on percussion, bass, chords
  and complex material with raw and matched files; report convergence rather
  than simply counting increasing distortion. Preserve old state interpretation.
- The current interface does make recording and generational progression
  discoverable; the larger problem is predictability of interactions. More
  knobs would not correct the clock or explain the capture balance.

### Gaps

- No newly assessed musical audition, Cubase capture/automation performance,
  exact-perceived-pitch study across Speed/Reverse/Pitch, or real-project recall
  trial is supplied by these notes. The existing music-machine checklist is a
  protocol, not a completed user study.

## Does Field hold and transform energy as the interface promises?

### Takeaway

Field is a coherent moving feedback network, but FOREVER is a bundled
transformation, not a lossless freeze. Its two central internal meters are not
direct measurements of total stored energy or actual decay, which can obscure
why a held sound changes or disappears.

### Cited Findings

- The eight-line Householder network has 31.1–82.1 ms base delays, continuous
  and held-offset delay motion, low-pass absorption, nonlinear writes and
  +7/+12-semitone dual-head pitch returns. This already provides spatial
  feedback, interpolation smear, pitch accumulation and signal-dependent
  saturation; it is not merely a dry/wet conventional delay.
  [Core](../../apps/field/field_processor.cpp), lines 10–20 and 200–312;
  [topology decision](../../docs/adr/0015-field-stored-energy-topology.md).
- Mass maps ordinary retention from 0.70 to 0.985 through a power-1.35 curve.
  FOREVER approaches 0.99935 but simultaneously adds 0.14 to Grain, Pitch and
  Motion, capped at one. Hence zero user Grain/Pitch/Motion does not mean zero
  held transformation. Pitch also reduces ordinary feedback by
  `(1−0.11*effectivePitch)` before its 0.095-scaled return is added.
  [Core](../../apps/field/field_processor.cpp), lines 202–217.
- Damping, moving linear interpolation and `tanh(clamp(write,−4,4))` remain in
  the held loop. Input injection falls from 0.24 to 0.14. The hold is neither
  a captured independent audio object nor a lossless resonator. The actual
  result depends on spectrum, amplitude and modulation, not the Retention
  scalar alone. [Core](../../apps/field/field_processor.cpp), lines 231–285.
- At full FOREVER, Mass no longer affects the retention interpolation but
  still darkens the feedback cutoff by up to 1600 Hz. A decay-named control
  therefore changes role in the main performance mode. All continuous values,
  including FOREVER's interpolation, use a 35 ms exponential smoother.
  [Core](../../apps/field/field_processor.cpp), lines 108–114, 188–217,
  272–276; [parameters](../../docs/products/field/PARAMETERS.md).
- The “Stored Energy” scalar is an exponentially smoothed square of
  `fieldMono = sum(lineOutput)/8`, followed by square root. Opposing line
  outputs can cancel before squaring although line buffers remain occupied.
  It is a mono-tap RMS-like proxy, not the sum of squared line states. The
  coefficient 0.001 is sample-rate dependent. The GUI scales it by 12 and
  clamps the bar; it uses a 0.00001 threshold for “HOLDING AUDIO” versus “HOLD
  ARMED / FEED AUDIO”. [Core](../../apps/field/field_processor.cpp), lines
  248–254 and 308–312; [editor](../../apps/field/field_plugin.cpp), lines
  187–193, 216–229 and 349–359.
- “Retention” displays the untrimmed coefficient, not measured decay and not
  the final feedback gain. It spans 0.70–0.99935 in a zero-to-one bar, making
  normal-state differences occupy only its top 30%. This is actual DSP-derived
  data, but the label and visual scale overstate what it explains.
  [Core](../../apps/field/field_processor.cpp), lines 207–217 and 311–312;
  [editor](../../apps/field/field_plugin.cpp), lines 191–193 and 216–229.
- Grain changes delay offset depth; Motion changes its update rate, smooth
  depth and sine rate. Pitch is the amount of fixed +7/+12 return, not a pitch
  selector. MIDI starts a single short decaying sinusoidal exciter; successive
  note-ons replace its frequency/envelope, not a polyphonic chord bank or
  tuned FDN. [Core](../../apps/field/field_processor.cpp), lines 146–165,
  219–262. The Field knob explicitly overrides the generic shared Pitch tooltip
  with “Amount of pitch-shifted audio returning into the field, not a tuning
  control.” This is not an unresolved tooltip collision, although the fixed
  intervals are not named there. [Editor](../../apps/field/field_plugin.cpp),
  lines 137–146.
- Blend is equal-power cosine/sine, not linear. Default 48% retains substantial
  dry attack and may conceal quieter changes in the field; that is a hypothesis
  requiring wet-only and dry/processed matched comparisons, not justification
  for blindly increasing wet gain. Output is a separate trim.
  [Core](../../apps/field/field_processor.cpp), lines 288–305;
  [defaults](../../apps/field/field_processor.hpp), lines 9–19.
- Schema 1 serializes controls only. Reopening a session does not restore the
  resonant material that made a FOREVER performance interesting. The UI has a
  useful Clear Field action that does not change the hold control, but that
  non-parameter command is a callback-boundary request rather than an
  automatable sound-memory event. [Adapter](../../apps/field/field_plugin.cpp),
  lines 265–270, 454–475 and 531 onward;
  [state contract](../../docs/products/field/STATE_FORMAT.md).

### Inferences

- **Smallest high-confidence improvement:** label the current meter honestly
  or replace it with an explicit bounded line-energy observation and distinguish
  coefficient retention from measured decay. This improves predictability
  without new musical parameters. Do not claim total buffer energy from eight
  taps unless that is what the label says.
- **Hold experiment:** compare current bundled hold with retention-only hold
  that honors zero Grain/Pitch/Motion, using identical impulses/chords/noise,
  30–120 seconds after input stops, fixed raw gain and matched listening copies.
  Preserve nonlinear safety and bounded state. Report spectral decay and
  movement independently; a longer tail alone is not success.
- **Further research:** test if the fixed upward intervals constrain useful
  harmonic contexts. A few isolated downward/non-octave returns are enough to
  investigate, not grounds for a large algorithm menu. Existing artists may
  value this brightness, so the current upward architecture is a viable control.
- Stable snapshot recall of held material is a workflow issue, not proof that
  the reverb sounds weak. It needs explicit memory/state/version design, not
  silently reinterpreting schema 1 or changing output on old sessions.

### Gaps

- No rendered evidence in these notes establishes actual hold lifetime,
  perceptual depth, spatial appeal, alias levels, preferred pitch intervals or
  fatigue. Existing [cycle 01](../../docs/products/field/CYCLE_01.md) explicitly
  reports synthetic-fixture evidence without musical/DAW verdict. The
  [music-machine checklist](../../docs/products/field/MUSIC_MACHINE_TEST.md)
  remains a proposed protocol, not listener data.

## Which restrictions are safeguards, and which are musical choices?

### Takeaway

Bounded memory, finite output, callback ownership and versioned recall are real
engineering requirements. Exact memory counts, motion/interval choices, zero
latency and local CPU targets are design choices that should earn their musical
tradeoffs rather than automatically veto alternatives.

### Cited Findings

- Callback allocation, locking, I/O, lazy setup and unbounded state are
  prohibited; these requirements must remain intact in alternative DSP.
  [Real-time rules](../../docs/REALTIME_SAFETY.md).
- The nominal family memory budget is 256 KiB, yet Field has an explicit
  1.5 MiB allowance and Loop's snapshot implementation reserves roughly
  88 MiB at 48 kHz. These accepted exceptions demonstrate that a smaller
  numerical budget is not itself a hard acoustic or safety law.
  [Performance document](../../docs/PERFORMANCE_BUDGETS.md);
  [snapshot decision](../../docs/adr/0016-playable-defaults-and-tape-recall.md).
- Field rejected convolution, named algorithm banks, post-network grain/pitch,
  known-unit cloning and user-selectable intervals for prototype scope and
  artistic coherence, not because they are universally unsafe or inherently
  inferior. The claim that post-reverb blocks “sound assembled” is a design
  judgment without a supplied matched listening trial.
  [ADR 0015](../../docs/adr/0015-field-stored-energy-topology.md).
- Loop intentionally rejected an early worker/snapshot implementation until
  race-free recall existed; schema 3 subsequently solved that with preallocated
  snapshots and off-thread restoration. Old documentation must not be used to
  claim the problem still exists. [ADR 0011](../../docs/adr/0011-loop-generational-tape.md);
  [ADR 0016](../../docs/adr/0016-playable-defaults-and-tape-recall.md).

### Inferences

- Retain all safeguards while evaluating a narrowly changed signal law. Keep
  current code as a production baseline, because identical parameter IDs and
  serialized values alone do not guarantee identical old renders.
- Next acceptance should ask whether a player can predict and repeat an
  evolving loop or held field, then recover it in a session—not whether the
  renderer remained finite or a meter moved.

### Gaps

- No oldest-supported-machine/loaded-Cubase measurements or newly completed
  native accessibility test is supplied. Local benchmark values must remain
  separate from release promises and perceived responsiveness.
