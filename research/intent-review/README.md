# Musical intent review — N-01 and R-01

The [musician's brief](../../docs/MUSICAL_INTENT.md) is the artistic context.
The new feedback rejects Non-Machine N-01 as confusing and musically weak and
finds Resonator R-01 pleasant but insufficiently valuable. The 0.7.1 tests are
implementation evidence, not acceptance of those instruments.

This review changes no instrument DSP, public parameters, installed binaries or
saved projects. It uses the currently installed version's checked-out cores to
render controlled counterexamples. No human audition by the implementation
agent is claimed. The musician's feedback is the available listening judgment.

## What went wrong in the response to feedback

Successive updates added source choices, parameter exposure, capture operations
and helper text before demonstrating a convincing musical interaction. N-01
now has 424 host parameters. Making them visible has not established a clear
relationship between the musician's gesture and what is heard. R-01's excited
modal-bank architecture meets a technical description, but its additional
musical value remains unproven. Neither another preset nor a larger panel is a
sufficient response to this feedback.

The new brief describes six identities. N-01/R-01 are candidates beyond that
core family and must justify their separate roles. Maintain their current
session compatibility while reviewing them; do not preserve a weak research
interaction merely because a parameter contract was introduced prematurely.

## N-01: causes of sound are not the same as visible events

The adapter in `apps/non-machine/non_machine_plugin.cpp`:

- Runs host-authored events when DAW Sequence and usable playing transport
  are present. Independent default cycles are 4/3/5 quarter notes against an
  eight-quarter-note master phrase; the local and master cycles do not all
  coincide each phrase. This is a substantial rule before any deliberate edit.
- Also accepts C/C#/D pitch classes on every octave and MIDI channel. These
  trigger objects A/B/C independently of Event On, authored positions and the
  currently selected event inspector. MIDI always uses event slot 1's locks.
  Editing another selected hit therefore need not change what MIDI auditions.
- Accepts explicit HIT requests using the selected event. HIT and MIDI are
  consequently different sound-edit contexts, despite both being auditions.
- Can play immutable local or master captures after source edits. Continue
  advances the hidden underlying phrase; Suspend pauses it. Releasing, rather
  than editing the source, is what exposes the new underlying state.
- Applies structural Void to the sequenced path, while manual audition bypasses
  it. Its event trace records automatic hits, not manual triggers or every
  audible onset produced by repeating captured audio.

These are implemented rules, not proof of an accidental self-oscillation bug.
Together they explain why a score plus many parameter fields is insufficient
feedback for the musician. Signal ownership must be explicit. A tag such as
“source event / MIDI / audition / local captured audio / master captured audio”
needs to follow the audio engine's accepted state, not an editor guess.

The new Pressure/Metal/Noise/Broken choices are mostly oscillation, modulation,
noise, gating and exponential amplitude decay. A name such as Metal does not
establish a distinctive excited physical body. Most new hits restart source
state; the default outgoing crossfade tail is only 2 ms. Pitch/color/decay locks
and material macros need evaluation as one coherent object, not more independent
ways to make it unusual.

### Smallest interaction worth evaluating next

Use one known object, an explicitly chosen trigger source, and a plainly
readable score. Default to deterministic authored events, with probability 100%,
one repeat, one condition and aligned cycles. Let the musician introduce one
timing transformation and predict its return. Keep silence empty. One direct
object-shaping gesture must change a coherent sound; optional hit locks must not
make the basic sound editor depend on an invisible audition slot.

This is a proposed evaluation setup, not a mandate for a new control layout or
an automatic shipped change. The actual next task is: create four deliberately
placed impacts, change their physical character, add one controlled accelerating
repeat, and correctly identify why each sound occurred. If the musician cannot
predict or author it, reopen the interaction model.

N-01's defensible differentiator is temporal composition and explicit nested
audio ownership. Invented percussion already belongs to I-01. Before continuing
N-01 as another drum instrument, compare this task against I-01 into L-01. Fold
or defer redundant capabilities if the separate instrument offers no advantage.

## R-01: stored energy versus material consequences

The current playable core has four chord tones with three modes each. At fixed
tuning its modal updates and coupling are linear rotations plus excitation,
followed by decay. Each mode receives the same excitation weight. Shared memory
keeps the quadrature state; it does not make material stiffness, damping or
coupling depend on the material's history. Partial memory scales old state;
Overlap uses two banks. Retune preserves state while changing its frequencies.

There is an energy ceiling and a final output nonlinearity; the full plugin is
not globally linear. The controlled comparison below avoids safety intervention
and measures the modal core before the plugin's final tanh. It does not claim
that every chord change, macro automation or high-energy condition is additive.

A linear resonator can be expressive. Linearity is not itself a defect or a
requirement to add distortion. This evidence shows why “shared state exists” is
not enough to claim the intended harmonic-memory value. With current excitation
and voicing, the prior energy can amount to another decaying chord response.
The user's assessment that it is cute but insufficient remains decisive.

### Competing directions, not a predetermined implementation

1. Keep the linear bank and prove an immediate excitation/retuning/energy-shedding
   performance gesture. This may need better voicing and excitation rather than
   another algorithm. Retuning existing energy without a new strike is already
   available and must be evaluated musically.
2. Compare an energy-dependent material model: controlled changes in damping,
   modal relationships or transfer as energy accumulates. Demonstrate the extra
   value at matched level with reproducible gestures, without turning every
   sound into distortion or unstable feedback. No such candidate is built here.
3. Defer the standalone instrument and use useful resonating-body work inside
   Impulse or Field if separate R-01 cannot demonstrate its own musical logic.

The evaluation should ask whether a second excitation has an audible,
controllable consequence in the existing material; whether a small gesture
moves between a substantial stab and an evolving body; and whether this is worth
playing instead of Sequence plus Field. “More modes” is not an acceptance test.

## Completed diagnostic experiment

The isolated `intent_audit` target uses only the current N/R C++ cores and the
existing audio laboratory helpers. No JUCE, external dependency or product build
is required.

```
cmake -S research/intent-review -B build-intent-review -DCMAKE_BUILD_TYPE=Release
cmake --build build-intent-review
ctest --test-dir build-intent-review --output-on-failure
build-intent-review/intent_audit build-evidence/intent-review-01
```

Use a fresh export folder. `INTENT_SANITIZERS=ON` builds with address/undefined
behavior sanitizers. The equivalent direct clang++ build was used locally.

Actual results at 48 kHz:

| Probe | Result | What it establishes |
|---|---|---|
| N empty score, no manual input | Peak exactly 0 | No autonomous audio in this controlled empty case |
| N empty score plus a manual hit at 1 s | Peak 0.270243; automatic event trace has zero hits | Score markers alone do not explain manual sound |
| N four authored hits versus the same hits with captures | Same four event records; audio RMS difference 0.096139 | Capture ownership changes the audible result independently of the event score |
| R Shared, two identical fixed-chord excitations versus sum of isolated responses | Relative residual -151.929 dB; max error 8.38e-9; zero safety interventions | The tested shared modal response is additive to float precision |

Nine actual 8-second stereo Float32 WAVs are exported with measurements and a
sample-timestamp cause script. One common scale per instrument keeps peaks at
or below 0.7 while retaining the real silence and level relationships. They are
not individually loudness matched and are not a musical preference comparison.

The R examples include first/second strikes alone, two strikes with Shared or
Reset memory, and retuning existing energy at 2 s without another excitation.
The N examples include silence, manual input outside the score, a sparse authored
score, and unchanged authored events with local/master captured repetition.
`n-causes.csv` is a scripted command/event ledger, not a claim that the current
plugin exposes a complete causal trace.

These results explain mechanisms. They neither demonstrate successful artistic
identity nor substitute for listening. No new DSP sound engine, interface build
or VST3 deployment follows automatically from them.

## Apply the brief to the six-instrument family

Use pressure/weight as Density's listening criterion; timbral substance for
Harmonic; performed electrical articulation for Sequence; inherited changes for
Loop; distinctive excited objects for Impulse; and evolving retained spatial
energy for Field. These are musical criteria, not a fixed synthesis recipe.
The recent literal phrase-capture addition to Field should also be judged by
whether it serves space that remembers or merely duplicates Loop. This review
does not silently remove it or claim it passes the revised brief.

The next experiment should be small enough to judge a meaningful gesture.
The engineering gates remain necessary, and musical preference must be reported
separately. Do not use another successful build or finite-output measurement as
approval to ship the next instrument revision.
