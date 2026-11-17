# C-01 — Contamination: first comparative cycle

2026-11-16. This cycle delivers an independent prototype, comparative renders,
measurements, a native interface and an isolated playable VST3 candidate.
**Musical acceptance is pending listening.** No claim of sounding like a reference
recording, compelling dry chords, physical weight or successful usability follows
from the engineering checks.

## Repository findings

The family separates framework-independent C++20 processors from JUCE adapters.
`CMakeLists.txt` pins JUCE; `libs/parameters` provides strict decimal entry,
precision sliders, state validation, final Master and experimental instrument
adapters. `docs/PARAMETER_MODEL.md`, `docs/UI_SYSTEM.md`, product specifications
and state formats establish stable host IDs, explicit response revisions,
host-notifying presets and a restrained single-panel interface. R-01/N-01 labs
establish isolated CMake research targets, float WAV packs, event traces,
observations, obscured listening IDs and separation of measurement from preference.

The workspace already contained extensive unrelated product work. This cycle
adds only `research/c01`, `apps/contamination`, and `docs/research/c01`; it does
not edit existing product processors, root CMake, shared UI or installer. The
family comparison links the current local versions and must therefore retain
source hashes, not just Git HEAD. No commits, installs or deployment occurred.

Sequence now has eight-voice polyphony as well as its original programmed mono
voice. Merely adding polyphony would duplicate existing work. Resonator's modal
bank stores ringing harmonic energy and has a limited held-chord representation;
C-01 must keep full individual MIDI notes and make memory affect *new generation*.
Field stores delayed/spatial energy; it is excluded from C-01's sound engine.
Harmonic shapes input spectrum and Density responds to input dynamics: neither
is an oscillator network with note-dependent state. Their drive and master-level
separation is retained as an interaction principle, not reused as C-01 DSP.

## Research basis and exclusions

[Higher-Order Frequency Modulation Synthesis](https://arxiv.org/abs/2305.07909)
provides a primary treatment of feedback FM/PM and discrete oscillator
formulations. It motivates comparing feedback placement, but this prototype
uses an invented bounded reciprocal phase return, not that paper's full method.

[Reducing the aliasing of nonlinear waveshaping using continuous-time convolution](https://www.dafx.de/paper-archive/2016/dafxpapers/20-DAFx-16_paper_41-PN.pdf)
shows why sampled nonlinearities require antialiasing attention and motivates
ADAA as an alternative to brute oversampling. Applying a memoryless correction
inside a recurrent oscillator would change its dynamics; this cycle instead
measures 1/2/4/8× generation plus FIR decimation. ADAA remains a follow-up, not an
unimplemented claim in the engine.

[Lyapunov Function for the Nonlinear Moog Voltage Controlled Filter](https://arxiv.org/abs/2104.04376)
illustrates that stability of a nonlinear resonant system requires a topology-
specific argument. Its proof is not applicable to our integrators. A resonant
ladder/SVF would add solver, tuning and oversampling decisions before proving
an independent musical benefit, so the first candidate uses bounded convex
updates instead. It does not claim to implement an analogue SVF.

[Real-time Physical Modelling for Analog Tape Machines](https://www.dafx.de/paper-archive/2019/DAFx2019_paper_3.pdf)
distinguishes history-dependent physical magnetization from static saturation.
C-01 borrows the question of stateful response, not a tape emulation or
Jiles–Atherton model. Its memory is explicitly an excitation-driven reservoir.
These sources establish engineering possibilities; they do not validate C-01's
artistic premise or certify the implementation.

## Smallest viable architectures

| Candidate | Internal relation | Advantage to test | Cost / reason not to lead |
|---|---|---|---|
| Single sine | One harmonic source, output saturation | Minimum dry low-frequency reference | Little harmonic articulation |
| Feed-forward pair | Detuned sine phase modulator, post-voice saturation | Controlled harmonic density, simple reference | Drive cannot feed back into generation |
| Reciprocal phase pair | Previous bounded oscillator return modulates its partner's phase | Noise/drive change generation directly; few states | Feedback and high-register aliasing need measurement |
| Filter return | Two-integrator output returns into oscillator phase | Frequency-selective loading of generation | More parameter-dependent; easy to drift toward a familiar filter polysynth |
| Nonlinear resonant SVF/ladder network | Resonant/filter feedback core | Self-oscillation, strong operating transitions | Deferred: numerical solver, tuning and topology cost |
| Shared audio saturator | Sum voices into nonlinear audio element | Strong chord-dependent intermodulation | Deferred: chord-size gain changes and loss of independent articulation |

The **reciprocal phase pair with hybrid operating memory** leads the development
candidate. This is a provisional engineering choice: it exhibits causal memory,
noise affects generation, boundedness is transparent, and the filter-return
alternative has not established an advantage. The measured centroid difference
between phase and filter-return candidates alone is small; no preference is
claimed. Keep the lab alternatives until listening adjudicates them.

## Implemented topology

Each of twelve voices has two sine phases (nominal ratio 1:1.003), one slow
correlated drift state, seeded noise, an amplitude ADSR, two bounded spectral
integrators, and local excitation/bias memory. A one-pole, 5 kHz return of the
oscillator enters tanh with Pressure-dependent gain. This return bends the
partner oscillator's phase, which then modulates the carrier. Contamination
injects correlated noise before that return nonlinearity. There is no separate
noise layer in the product mapping. Fundamental sine support remains in parallel
inside the oscillator expression, not through an external effect.

The spectral path uses two cascaded convex integrator updates. Its saturated
input includes a small state return; its output feeds phase only in the separate
FilterReturn experiment. A bounded summed energy/bias reservoir loads later
voices through phase depth and operating bias. Local memory also loads the
return; global memory measures rectified voice energy plus the rectified
difference between two summed frequency bands. Different notes therefore excite
different shared conditions. Stereo placement is a small fixed function of note;
there is no chorus, delay, reverb, wavetable, chord generator or sequencer.

Pressure controls the strength of the same nonlinear interaction, with square-
root drive normalization. Contamination controls noise *inside* that interaction.
Instability controls slow frequency and ratio departure. The current instability
model is correlated drift interacting with the nonlinear return; it does not
establish bifurcations, chaos or self-organized resonant transitions. Memory
controls participation and relaxation of excitation history. Motion is deliberate
envelope-driven phase depth with state-dependent spectral opening, rather than
another independent random source. CC1 adds phase depth and channel pressure
adds operating pressure.

Every state and its lifecycle is specified in [CONTRACT.md](CONTRACT.md).

## Experiment coverage and limits

| Study | Implemented comparison | What the evidence can establish |
|---|---|---|
| C1 | Single sine, feed-forward pair, reciprocal and filter-return chords | Density/centroid/crest changes; dry preference needs listening |
| C2 | Feed-forward output saturation versus internal oscillator/return nonlinearity | Different spectra and transients under common notes; not an exact circuit ablation |
| C3 | Phase, frequency, bias, feedback and output injection × white/correlated/high-pass noise | Structural noise versus added noise; not all placements belong in product |
| C4 | 0/40/100% correlated instability | Repeat difference, deterministic restart; no proof of chaos |
| C5 | Reset/local/shared/hybrid, sixteen identical chords, return to clean | Prior excitation affects later notes; zero-memory repeat consistency |
| C6 | 0/50/100% Pressure | Spectral/crest and level changes, separately matched audio |
| C7 | Minor/suspended/extended/cluster sequence; local/shared/hybrid | Response to harmony and shared loading; direct shared audio clipping deferred |
| C8 | Sparse three-second spacing, ten-second sustain, velocity/overlap, CC1/pressure/bend, abrupt drive automation | Scheduling, boundedness and performance range; player evaluation pending |
| C9 | Native resizable single panel with measured state readouts | Build/layout/recall checks; musician and accessibility evaluation pending |
| C10 | C-01, current Sequence Poly, Sequence→Field, R-01 shaped modal excitation | Concrete family references; differentiation is a listening judgment |

All main takes are 14 seconds, 48 kHz, stereo float32. Standard score: sixteen
C minor-seven chords at 750 ms intervals, velocity .8, 140 ms gate. The extended
score changes every four repetitions through minor, suspended, extended and
clustered structures. No voicing is baked into the product. The sparse score
plays at 0/3/6/9 seconds; sustained notes last ten seconds. Traces are retained.

Noise is deterministic xorshift64 with different note seeds. Correlated noise
is a 180 Hz one-pole process normalized for equal stationary variance across
sample rates/factors; white/high-pass variants retain their spectral differences.
Its settling transient is not loudness-normalized sample by sample. At Memory=
Instability=0, resetting seeds at note-on makes contamination reproducible.
Instability>0 retains random/drift state through successive notes; global reset
remains deterministic. No wall-clock randomness is used.

The pack provides raw and RMS-matched takes. Matching targets −20 dBFS RMS,
with −1 dBFS peak ceiling, so some transient-heavy takes may fall below the RMS
target. This is not LUFS matching and does not remove every loudness cue. IDs
are obscured with a reproducible permutation; an answer key is separate from a
blank response sheet. They are not a claim of a double-blind listening trial.
Metrics describe the first 65536-sample spectral window and whole-take RMS/crest;
they do not measure subjective darkness or weight. The first/last difference
is normalized waveform RMS difference and is meaningful for the identical-
chord studies, not the sparse/sustained/performance takes.

The alias study disables noise, drift, memory, motion and detune, sustains one
note and excludes ±5 FFT bins about expected harmonics. Remaining energy is an
**off-harmonic leakage proxy**: window sidelobes, feedback behavior and folding
can all contribute. It is not a complete alias-power separation or a reference
null. Tests include MIDI 69/93/105 at moderate/full pressure, 1/2/4/8×. At high
pressure and MIDI105 the improvement from 4× to 8× remains significant; 4× is a
CPU/quality development compromise, not an alias-free claim. Full-register,
polyphonic, moving-parameter alias audibility remains release work.

## Listening procedure and decision gates

Listen to matched takes before reading the key. Rate harmonic legibility,
physical weight, integration of noise, meaningful relationship between events,
preference and confidence separately; allow “no difference”. Then compare raw
levels, long sustains and one chord every three seconds. Test a single MIDI note
and arbitrary chords in the plugin; do not infer quality only from the lab's
minor-seven voicing. Memory should be heard as prior excitation changing the
next response, rather than different random seeds. Lower Memory/Instability,
then test repeatability after their transition. CLEAR MEMORY should be an
intentional discharge while held notes continue.

C2 requires deciding whether internal drive is musically better, not merely
measurably different. C6 must establish whether moderate drive adds weight:
current measurements show increasing centroid and decreasing raw RMS, so the
word “weight” remains especially unproven. C7 must establish that shared state
adds useful chord interaction without obscuring harmony. C9 needs a musician to
reach a short stab and a sustained texture without instruction beyond visible
cues. C10 needs preference/confidence scores against current family patches,
including improved comparator patches if the first references are weak.

Do not freeze the topology or approve factory presets until these gates pass.
