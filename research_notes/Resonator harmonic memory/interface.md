# R-01 interface and differentiation notes

This is a bounded repository-based design study, not a listening result or a
claim about Porter Ricks' production methods. The source of truth is Aste's
current implementation at baseline `8a603dc`, plus the parent's isolated R-01
laboratory. Proposed interactions remain distinct from implemented controls.

## Existing family boundary

| Existing behavior | Reuse or avoid duplicating | R-01 distinction to test |
|---|---|---|
| Sequence is monophonic and begins with two oscillators plus sub, a nonlinear filter and amplitude contour | Keep exact entry, explicit focus, MIDI handling and source-independent host boundary conventions; do not port the oscillator panel | Resonant frequency/damping relationships generate the chord from unpitched input |
| Field's eight-line orthogonal feedback network stores energy and its FOREVER control changes retention | Reuse energy-state communication and explicit empty-held feedback; do not add a reverb panel to a chord synth | A visible four-tone resonant structure can be retuned while retaining existing state |
| Field pitch return is a fixed fifth/octave pair; MIDI adds a tuned sine excitation rather than retuning the network | Do not claim that all energy memory is novel | Coupling and shared-state chord transitions change the tuned generator, not merely its wet effect |
| Family UI exposes musical phenomena, one scalable panel, text entry, focus and accessibility descriptions | Preserve near-black/off-white hierarchy and bounded state-driven motion | Show excitation, harmonic relationships and memory consequences instead of oscillator/filter/envelope controls |

Sources: [Sequence spec](../../docs/products/sequence/SPECIFICATION.md),
[Sequence parameters](../../docs/products/sequence/PARAMETERS.md),
[Sequence core](../../apps/sequence/sequence_processor.hpp),
[Field spec](../../docs/products/field/SPECIFICATION.md),
[Field parameters](../../docs/products/field/PARAMETERS.md),
[Field core](../../apps/field/field_processor.cpp),
[UI system](../../docs/UI_SYSTEM.md).

Four independent Sequence instances are a fairer chord comparison than one
monophonic Sequence instance. Route their sum through one Field instance and
retain the actual source MIDI/automation recipe. A rendered signal comparison
cannot measure editing effort; that needs a separate task trial.

## Competing layouts

1. **Twelve-mode mixer.** Precise, but permanently exposes frequency, gain and
   damping for twelve modes. Rejected as the default: it makes a synthesis lab
   the primary performance workflow.
2. **Conventional keyboard plus synth macros.** Familiar, but suggests each note
   starts a self-contained voice and conceals shared-state harmonic memory.
   Rejected as the sole representation; MIDI remains a supported input method.
3. **Harmonic structure plus selected-mode inspector.** Four tone groups, each
   with three meaningful energy indicators; one selected item exposes details.
   Selected for a static study because the musical structure and memory policy
   remain continuously legible.
4. **Abstract physical sculpture.** Potentially attractive, but displacement
   and color can easily become fabricated activity unrelated to DSP state.
   Rejected until a real measured quantity and an actionable gesture justify it.

The waveguide comparison must not borrow modal bars that it cannot measure.
Four line-energy summaries are honest; a full FFT would be an output analyzer,
not a representation of internal independent modes. No architecture switch
should survive into a product merely to preserve every research branch.

## Minimal first instrument hypothesis

Four-tone structure, three modes per tone, a small exciter selector, excitation
amount, Decay, Damping and bounded Coupling; one explicit memory policy and Clear
Energy. Root/interval/cents entry and selected-mode damping provide progressive
precision. Persistence must not secretly inject energy. A small set of minor,
suspended, extended and ambiguous examples is enough to test the sound; it is
not a chord-library product.

The central performance gesture should be understandable without an animation:
excite, change the structure while energy remains, then choose whether the next
excitation resets, partially damps, shares or overlaps that state. Whether this
deserves a dedicated instrument is still unknown.

## Small task protocol

Conduct only after audio exists and an actual operator can assess it. Preserve
blank response sheets until then; these notes contain no fabricated outcomes.

1. Show an unlabelled-by-engine, level-matched modal/waveguide pair driven by
   the same excitation. Record perceived harmonic structure, attack, evolving
   spectrum, controllability and confidence separately from preference. “No
   reliable difference” and “no preference” are valid answers.
2. Give the wireframe to a new operator. Ask them to identify what sets the
   chord and what supplies energy. Record their answer before explanation.
3. Ask them to change a minor snapshot to a suspended/ambiguous structure
   without re-exciting. Can they predict whether previous tuning survives?
4. Repeat the phrase in Reset, Shared, Partial and Overlap. Ask which policy
   was heard and which state is still alive. Reveal the policy only afterward.
5. Ask for a short stab, then a retained evolving texture, with no delay added.
   Count required edits, recovery from mistakes and unintended energy buildup.
6. Recreate the useful result with four Sequence instances into Field. Record
   time, edits, automation lanes and repeatability—not only which sample sounds
   more dramatic. Match levels and preserve both recipes.
7. Repeat core tasks keyboard-only and with a screen reader once there is a
   native prototype. Static SVG inspection is not an accessibility pass.

Advance only if the operator can predict memory behavior and repeat a useful
composition that the family comparison cannot achieve as simply. If the sonic
advantage disappears after level matching, or control demands detailed mode
engineering, keep the lab and do not proceed to a plugin.

## Artifacts and limits

[Interaction specification](../../docs/research/r01/INTERACTION.md),
[excitation view](../../docs/research/r01/excitation.svg),
[memory transition](../../docs/research/r01/memory.svg).
The SVG values are illustrative. No native event handling, host automation,
meter handoff, adaptive scaling, physical model, subjective listening result or
accepted product parameter contract is claimed.

Both original SVGs pass `xmllint --noout`; repository documentation-link checks
pass. Quick Look rendered scratch square-canvas copies for full-width visual
inspection because its ordinary thumbnail path crops these landscape SVGs.
The source artifacts retain their intended 1280×800 dimensions. Scratch PNGs
are under ignored `build-ui-review/`; they are not screenshots of a running UI.
Calculated palette contrast against the darker/lighter panel backgrounds is
13.64:1 or better for primary text, 8.09:1 or better for secondary text, and
4.02:1 or better for control outlines. This checks the specified flat colors,
not native rendering, scaling, focus behavior or assistive-technology support.
