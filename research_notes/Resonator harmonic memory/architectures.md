# Resonator architectures and harmonic memory

Research notes, 2026-11-14. Public DSP chapters below were actually retrieved and
read; no recordings were assessed and no numerical prototype result is asserted
here. Repository facts refer to Aste baseline
`8a603dcbc59b52a58637fe6305676efc5a406f80`. Equations and candidate choices under
Inferences are research recommendations, not adopted product contracts.

## Which resonator architecture gives the smallest controllable experiment?

### Takeaway

A small complex modal bank is the clearest first instrument model because
frequency, damping and stored state are directly inspectable. A fractional-delay
feedback bank is a valuable competing candidate, but interpolation loss and
dispersion must be measured rather than hidden behind equal knob positions.

### Cited Findings

- Modal representation realizes a linear system as parallel first-/second-order
  sections; the state-space formulation can share modal poles across inputs and
  outputs. This supports a compact bank without an oscillator/filter voice per
  chord tone. — Julius O. Smith, [Modal Representation](https://www.dsprelated.com/freebooks/pasp/Modal_Representation.html),
  [State Space to Modal Synthesis](https://www.dsprelated.com/freebooks/pasp/State_Space_Modal_Synthesis.html).
- A complex one-pole resonator has pole `p = r exp(j theta)` and an exponentially
  decaying sinusoidal impulse response. Unit radius yields a sustained complex
  sinusoidal oscillator; real and imaginary components provide quadrature state.
  — Smith, [Complex Resonator](https://www.dsprelated.com/freebooks/filters/Complex_Resonator.html).
- Fixed linear delay interpolation is an FIR filter with frequency-dependent
  magnitude response. Inside a near-unity feedback loop its amplitude error
  changes the decay; allpass interpolation removes that magnitude error at the
  price of delay error. — Smith, [Fractional Delay Filtering by Linear Interpolation](https://www.dsprelated.com/freebooks/pasp/Fractional_Delay_Filtering_Linear.html).
- The first-order allpass `H(z) = (eta + z^-1)/(1 + eta z^-1)` has unit magnitude
  at fixed coefficients. Its low-frequency delay is approximately
  `(1-eta)/(1+eta)`, not a frequency-independent delay. The chapter warns against
  the near-zero-delay pole/zero cancellation and against treating its recursive
  state as random-access interpolation. — Smith, [First-Order Allpass Interpolation](https://www.dsprelated.com/freebooks/pasp/First_Order_Allpass_Interpolation.html).
- Stiff-string dispersion can be represented by allpass filtering in the
  waveguide loop: frequency-dependent propagation delay changes partial spacing.
  This is distinct from frequency-dependent damping. — Smith,
  [Stiff String Synthesis Models](https://www.dsprelated.com/freebooks/pasp/Stiff_String_Synthesis_Models.html).
- Orthogonal/unitary matrices provide a sufficient lossless mixing construction
  for feedback-delay networks before damping; Householder reflection is one
  efficient example. — Smith, [Choice of Lossless Feedback Matrix](https://www.dsprelated.com/freebooks/pasp/Choice_Lossless_Feedback_Matrix.html).

### Inferences

- Start with a bounded bank of roughly twelve modes: four chord relationships
  and three partials per relationship. Use small explicit interval arrays rather
  than a chord database. Minor, suspended, extended and ambiguous examples can
  be represented by intervals such as `{0,3,7,10}`, `{0,5,7,12}`, `{0,3,7,14}` and
  `{0,2,7,9}`. These are examples to audition, not a musical selection.
- For one mode use `z[n+1] = r R(theta) z[n] + b u[n]`, where `z` has two real
  coordinates. With amplitude T60 in seconds,
  `r = exp(-ln(1000)/(sampleRate*T60))`. Frequency changes rotate retained state;
  damping changes contract it. Independent partial damping and per-mode output
  gains make spectral evolution explainable.
- For a coupled modal update `z' = Q D R z`, let `Q` and `R` be orthogonal and
  diagonal damping satisfy `0 <= d_i <= r_max < 1`. Then, without excitation,
  `||z'||_2 <= r_max ||z||_2`. The same bound survives arbitrary sample-to-sample
  choices of these orthogonal rotations. Pairwise Givens rotations give a small,
  deterministic energy-transfer control without dense matrix machinery.
- This autonomous norm argument is not a claim of BIBO safety at infinite
  persistence. With bounded injection `||b u|| <= U`, a bound scales as
  `U/(1-r_max)`; at `r_max = 1`, coherent repeated excitation can grow without
  bound. Choose a strict retention ceiling and/or an explicit energy governor.
  Count and report governor activations; finite clipped output alone does not
  establish a healthy resonating system. Numerically guarded states must not
  silently conceal a failed stability experiment.
- A bounded feedback-delay bank is a contrasting chord architecture. It gives
  many related partials per loop and efficient string-like resonance, while the
  modal bank permits independent partial tuning/damping. Give both the same
  input waveform, root progression and measured excitation energy. Match output
  level separately and retain raw files. Their available modes need not be
  identical; disclose the architectural difference instead of pretending an
  equal mode count or nominal decay makes them identical systems.
- A linear-interpolated delay candidate is a useful minimal baseline, not proof
  that all waveguides sound dull. Measure pitch and decay before judging it.
  A fixed allpass/dispersion comparison can follow if interpolation loss explains
  a result. Abruptly changing a recursive interpolator's coefficients requires
  separate transient and state-energy testing; fixed-coefficient allpass theory
  does not automatically prove a time-varying implementation safe.

### Gaps

- No oscillator/resonator topology guarantees an identifiable musical character.
  These notes cannot select modal versus waveguide by preference.
- No CPU, tuning error, damping curve or alias floor is inferred from textbook
  operation counts. The executable laboratory must produce those measurements.
- The proposed norm bound applies to its explicit state formulation, not an
  arbitrary biquad implementation or every time-varying delay network.

## What does retained harmonic energy actually mean?

### Takeaway

Retained energy alone is not a new interaction: a fixed linear resonator obeys
superposition. The useful research question is whether controlled retuning,
damping or redistribution of that retained state gives playable behavior that
independent old/new chord tails do not.

### Cited Findings

- A linear state-space system admits a modal decomposition; multiple inputs and
  outputs can share the modal state. Sharing memory does not by itself imply
  nonlinear interaction. — [State Space to Modal Synthesis](https://www.dsprelated.com/freebooks/pasp/State_Space_Modal_Synthesis.html).
- Aste Field already retains energy in eight moving delay lines. Its source
  uses Householder feedback, damping, bounded `tanh` delay writes and two
  +7/+12-semitone feedback pitch voices. FOREVER approaches retention 0.99935
  while modifying movement and injection. — [Field DSP](../../apps/field/field_processor.cpp),
  [Field specification](../../docs/products/field/SPECIFICATION.md).
- Field's MIDI exciter is a short tuned sinusoidal injection. `noteOn` replaces
  its current frequency/envelope; note-off has no special instrument meaning.
  It does not directly tune the network to arbitrary chord intervals. —
  [Field DSP](../../apps/field/field_processor.cpp),
  [Field specification](../../docs/products/field/SPECIFICATION.md).
- Field session schema 1 does not serialize delay memory or random state; a
  restored held control begins with empty audio history. A harmonic-memory
  instrument cannot infer saved state from its control values alone. —
  [Field state format](../../docs/products/field/STATE_FORMAT.md).

### Inferences

Use the same scheduled second excitation and measure/audio-render these policies:

| Policy | Explicit state action | Musical/control question |
|---|---|---|
| Reset | Clear state before each new excitation | Does a clean stab remove essential continuity? |
| Independent overlap | Keep old tuning/state in one bounded bank; excite a separate new bank | Does an ordinary overlapping chord tail already suffice? |
| Shared state | Retune existing state coordinates and add the new excitation | Is carrying energy into a new harmonic frame useful or merely an abrupt pitch bend? |
| Partial damping | Multiply retained state by a documented scalar before retuning/re-excitation | Can one memory amount make the transition predictable? |
| Coupled transfer | Rotate energy among selected mode coordinates; add excitation separately | Does controlled transfer add evolving timbre beyond retuning alone? |

- Include a null control: at fixed coefficients and without limiting, rendering
  excitation A plus delayed B through one bank should match the sum of separate
  A and B renders, within floating-point tolerance. If this fails unexpectedly,
  identify the nonlinearity/state reset rather than calling it harmonic memory.
- Fixed linear coupling still creates another linear resonator system. It may
  produce useful eigenmodes and beating, but should not be advertised as a new
  nonlinear interaction. Time-varying coupling/tuning and partial damping make
  the instrument's history-dependent control explicit.
- A bounded two-bank overlap reference needs an explicit replacement rule when
  a third chord arrives. Its voice stealing is a research limit, not unlimited
  polyphony. Match the same finite chord schedule for all policies.
- Separate note excitation from sustain behavior. A note can inject energy,
  note-off can reduce persistence without clearing the state, and a separate
  damp/clear gesture can explicitly remove memory. MIDI should report accepted
  notes and targets; malformed input must not quietly create a default chord.
- For near-infinite decay, test silence after an impulse, coherent repeated
  injection, sustained external input, frequent root changes, maximum coupling,
  damping endpoints and repeated note-on/off at all supported rates. Record
  pre-governor energy, output peak, state finiteness, governor count, remaining
  energy and recovery. A limiter that fires continually is an audible design
  choice, not evidence of a passive stable resonator.

### Gaps

- No saved harmonic-memory format is recommended yet. A future contract must
  choose between explicit empty-state recall and portable captured state, with
  sample-rate conversion and off-thread snapshot ownership specified.
- No assessed audio is available in these notes, so terms such as “physical,”
  “dub-like,” “expressive” or “useful beating” remain intended outcomes.

## How can differentiation and spectral safety be assessed honestly?

### Takeaway

Use both a source-controlled architecture test and an instrument-capability test.
Neither a spectral difference nor a long retained tail establishes superiority
over Sequence plus Field; composition effort and repeatable listening remain
necessary evidence.

### Cited Findings

- Sequence is one monophonic voice with two polyBLEP oscillators plus sub,
  nonlinear shared-state filter and ADSR; MIDI selects/transposes that voice.
  Holding a chord on a single instance does not produce four independent chord
  voices. — [Sequence specification](../../docs/products/sequence/SPECIFICATION.md),
  [Sequence DSP](../../apps/sequence/sequence_processor.cpp).
- Field already supplies deterministic moving retained energy, external audio
  excitation and a pitch-in-feedback structure; its nine parameters do not
  expose independent modal/chord frequencies. — [Field parameters](../../docs/products/field/PARAMETERS.md),
  [Field DSP](../../apps/field/field_processor.cpp).
- Existing Aste tests cover six supported measurement rates, irregular block
  sizes, finite hostile states and no C++ processing allocation. Field includes
  a minute-long fully pitched FOREVER case. — [Sequence tests](../../tests/sequence_tests.cpp),
  [Field tests](../../tests/field_tests.cpp).
- Aste research policy requires falsifiable questions, deterministic comparison,
  measurements and listening for perceptual decisions; numerical tests do not
  establish sonic quality. — [DSP research policy](../../docs/DSP_RESEARCH.md),
  [listening protocol](../../docs/testing/LISTENING.md).

### Inferences

- For R8 use four independently instantiated Sequence voices, sum them with
  documented headroom, then feed one Field. Use the same root/chord transitions,
  onset/off times and velocity sequence as R-01. Also render dry Sequence sum
  and Field-only excitation if feasible. Do not handicap the reference by
  letting one monophonic Sequence collapse every chord to its last note.
- First architecture test: identical pre-generated impulse, white noise,
  approximate pink noise, filtered/enveloped noise and finite granular burst
  samples feed modal and delay candidates. Save the excitation itself. Equal
  seeds are not enough when two generators consume random numbers differently.
  External excitation can initially be a declared deterministic test signal;
  do not imply imported recorded audio or a live host input was tested.
- Pitch error in cents, fitted decay by mode/band, measured late energy,
  spectral centroid and RMS/peak are useful diagnostics. They do not measure
  musical identity. Noise-slope claims need a measured bandwidth/window and
  rate-aware filters, especially if an approximate pink generator is used.
- A fixed linear modal bank does not create new frequencies beyond the input
  and its transient response. High-band energy from white-noise excitation is
  not itself aliasing. First test low-level stationary coherent tones and a
  linear-superposition null. Then test nonlinear safety/governor activation and
  time-varying tuning/coupling independently: these can create harmonics or
  sidebands that fold, even when each instantaneous mode frequency is below
  Nyquist.
- A useful alias estimate compares the target-rate render with an otherwise
  equivalent high-rate render that is properly low-pass filtered and decimated,
  with gain/group-delay alignment and a calibrated decimator error floor. Keep
  excitation bandwidth, physical durations and continuous control trajectories
  consistent across rates; reusing the same noise sequence at two sample rates
  is not the same excitation. Report reference residual as a combined
  rate-dependence/error measurement unless all non-alias contributors are
  isolated. A few FFT bins or a raw 48-vs-192-kHz sample subtraction cannot
  establish a comprehensive alias floor.
- Oversampling is therefore conditional, not a default feature. If a benign
  linear configuration passes but strong energy-governor/tuning cases fail,
  isolate the offending stage, compare filtered high-rate behavior and measure
  its CPU/latency trade-off before selecting it.
- Falsifiable music gate: give the same short chord/space brief to R-01 and
  Sequence→Field; record edit count/time, reliable reproduction of an intended
  transition, blind preferences and confidence. Require repeated preference
  or demonstrably easier control in at least two contrasting phrases without
  severe tuning/stability failures. “No reliable difference” and “use existing
  products instead” remain valid outcomes. These thresholds are proposed study
  rules, not results.

### Gaps

- No human listening, Cubase playability, supported-oldest-hardware performance,
  perceptual loudness equivalence or editing-effort study was performed by this
  researcher. The main laboratory must report its actual completed coverage.
- The safe next recommendation is a small modal-led comparative prototype,
  not a seventh plugin. Advance only if rendered/assessed material and control
  trials demonstrate a repeatable benefit over the existing family.
