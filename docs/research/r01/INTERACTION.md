# R-01 — excitation and harmonic-memory interface study

Status: static, illustrative wireframes and an interaction proposal. No native
editor, MIDI usability trial, VoiceOver result, host automation mapping, stable
parameter ID or finished product identity is implied. Values in the drawings are
examples, not measurements from a rendered fixture.

The sketches are deliberately ahead of the callable DSP surface: the current
lab supports per-tone cents and per-mode decay, but not arbitrary independent
per-mode ratio/fine-tune edits. Shaped noise uses a fixed 1.8 kHz low-pass, not
the drawing's proposed Color macro. Those fields are interaction hypotheses,
not implemented controls or reasons to expand the engine before listening.

- [Excitation and harmonic structure](excitation.svg)
- [Shared-memory chord transition](memory.svg)

## One screen, one question

The visible question is **what is being excited, and what happens to energy
already there?** A large Excite action introduces energy into a four-tone
structure. Twelve compact modal bars show three components per tone. The input
strip describes the excitation; the harmonic strip describes the resonating
structure; the memory strip explains the consequence of the next excitation.
This is not an oscillator/filter/envelope panel with new labels.

The proposed base panel is 1280×800. Near-black `#151719`, slightly raised
`#1d2023` panels, off-white `#eeeae2`, secondary text `#b2b7bb`, and `#777f86`
outlines follow the [family system](../../UI_SYSTEM.md). Grayscale deliberately
avoids choosing a permanent product accent. Typography is technical and uses
available monospaced fonts; no external font or asset dependency is introduced.
There is no decorative motion, 3D object, animated particle field or hardware
keyboard imitation.

## Excitation and structure stay separate

Excitation source, amount, duration and spectral shape form one region. The
source selector offers the implemented candidates—impulse, noise, short burst
or external input—rather than an open-ended sample browser. White/pink/shaped
noise are source details, not extra top-level engines. Exact options and labels
must follow the actual lab candidates selected for a future editor.

The visible instruction reads “Excite the structure. Change the noise, not the
chord.” Holding the Excite control must not silently turn a single impulse into
a sustained noise source. Repeated or continuous injection is an explicit
performance mode if later supported. External input shows “Listening to input”
and a real input level; unavailable routing shows “No input bus,” not a false
silent meter. The burst-length field is visibly inactive where it has no effect.

Root plus four interval fields define a small chord snapshot; interval zero
normally anchors the first tone. Note names, semitone offsets and cents are
available as exact values. A MIDI source selector explicitly distinguishes a
held-note set from a root-plus-interval snapshot. Accepted MIDI notes are shown
as text, including an explicit note-count limit and any rejected note. Do not
display a large chord library before the core engine earns one.

Changing an exciter does not alter resonator tuning. Changing harmonic structure
does not itself imply an excitation unless an explicit “Excite after edit” mode
is enabled. The default proposal separates these operations so a person can
hear retained energy move without adding new energy. The implementation must
publish whether a command retuned, excited, cleared or rejected; the UI cannot
infer that from its own value field.

## Modes and energy

In the modal candidate, each tone owns three modes. Their bar positions are
stable mode identities, not an unlabeled FFT. The fundamental's note/frequency
appears above the group. Selecting a bar opens a fixed-position inspector with
mode frequency/ratio, cents offset and damping; the other controls do not move.
The inspector reports the effective frequency when a proposed tuning is bounded
or refused, rather than pretending the requested pitch was accepted.

Energy height represents a normalized DSP state measure, such as `real² + imag²`
for a complex mode, with the scale declared by the engine. It is not dBFS,
acoustic loudness or joules. Input/output meters remain separate -60 to 0 dBFS
displays. “NO STORED ENERGY” is a useful empty state. A configured but empty
structure remains visible; bars do not invent activity while the host is stopped.

For a waveguide candidate, do not relabel twelve synthetic bars as measured
internal modes. Show four string/line energy summaries and their tuning instead;
the three-mode detail is unavailable with a visible explanation. Architecture
comparison is a laboratory selector, not a promised product quality mode. The
eventual instrument should expose one selected architecture unless listening
demonstrates that switching is a real musical action.

Coupling uses a bounded amount control with a plain description: “Transfer energy
between modes.” The illustration's thin connector identifies the selected pair;
it is not an animated signal path. A future moving connector requires a genuine
energy-transfer statistic, not a sine-wave animation driven by elapsed time.
Do not claim that stronger coupling always means more energy or brighter sound.

## Memory is an explicit action policy

The selected policy is always visible next to Excite. Each policy has a short
consequence sentence, not an ambiguous “poly/mono” switch:

| Policy | Consequence to display before the next excitation |
|---|---|
| Reset | “Clear stored energy, then excite the new structure.” |
| Shared | “Retune the current state; new energy enters the same system.” |
| Partial damping | “Reduce current energy, retune, then excite.” Show the retained amount and its units. |
| Overlap | “Keep the previous bank; excite a second bank.” Show both banks and the fixed voice limit. |

Coupling is an independent transformation of the resonator state, not a fifth
voice-allocation policy. The research can compare coupled Shared/Partial with
uncoupled equivalents without conflating two decisions in one dropdown.

The second wireframe illustrates a Shared transition: the current structure is
now F–A♭–B♭–E♭ and existing energy has been retuned into it. A short status line
states that the previous C-rooted chord is not an independently surviving voice.
This is a conceptual example; the actual lab's chord-transition timing and
state-mapping rule take precedence.

In Overlap, the same area shows CURRENT and PREVIOUS bank labels, separate
energy values and the voice-steal result. A third chord must not create hidden
unbounded voices. If the oldest bank is replaced, say “Previous bank replaced”
and expose the transition rule. In Reset, show “Energy cleared” briefly after
acknowledgment; do not animate a retained tail that no longer exists.

Decay is displayed in seconds with an explanation of the measurement convention
(for example, amplitude T60). Damping describes relative loss across modes.
Persistence is visibly distinct from adding continuous input: an empty held
system stays empty. If persistence remains below unity, say “Long retention,”
not “Infinite.” A safety intervention indicator shows when the engine bounds
energy; that is actionable feedback, not a decorative clip lamp. Clear Energy
is a separate, explicit action and never changes the chord settings.

## Precision, keyboard and feedback

Focus order: Excite, MIDI/snapshot source, root, four intervals, exciter source,
excitation amount/duration/color, memory policy, Decay, Damping, Coupling,
selected-mode inspector, Persistence, Clear Energy, Output. Within a tone group,
arrows select one of its three modes; Tab leaves the group. The group announces
“Tone 2, mode 3, effective 622 Hz, energy …” rather than an unlabeled bar.

Every drag has typed numeric entry and arrow-key alternatives. Fine drag uses
the host-standard modifier; exact cents are available without a modifier chord.
Invalid input stays visible with the constraint and the last accepted value.
Enter commits a field, Escape cancels only that edit, and double-click resets
only that control. Undo groups a drag into one change. Pitch edits expose a
requested/accepted value distinction when the DSP rejects or bounds a request.

Space triggers Excite only while that button has focus; it is not a global
transport shortcut. Key auto-repeat must not generate a retrigger storm. MIDI
note-on velocity and zero-velocity note-on semantics belong to the adapter, not
the paint code. Note-off/sustain-pedal behavior must be explicit in the candidate
playability contract and tested before the interface promises it. For a first
experiment, sustain-pedal support is not implied.

Each control has a visible label, stable accessible title/description, units and
accepted-state value. Selection, focus, active memory and safety intervention use
text and outline/fill, not color alone. Provide a text view of modal state rather
than requiring inspection of bars. Live energy updates must not continuously
interrupt a screen reader; announce changes to policy, accepted tuning and
errors on demand or politely. A reduced-motion preference freezes interpolation,
not the underlying measurement or its numeric text.

The proposed scalable range is 1040×680 to 1600×1000. Essential controls, current
structure and memory consequence remain visible at all sizes. Small-scale mode
targets retain generous hit bounds; cosmetic bars can be thin inside those
bounds. Readability and 44-pixel action targets take precedence over uniform
geometric shrinking. The SVGs show only the 1280×800 composition, not proof that
the other sizes work. Native Retina, contrast, focus, Cubase key routing and
VoiceOver checks remain required.

## UI/audio ownership proposal

The audio thread owns modes, delay history, excitation, accepted event order and
energy constraints. It publishes a fixed-size snapshot: current tuning, per-mode
or per-line energy, active-bank identity, input/output levels, last accepted
action and safety-intervention count. The UI consumes a lock-free, coherent
handoff at a bounded display rate (at most 30 Hz); it never reads mutable DSP
arrays directly or sends 30-Hz display values back into the sound engine.

The existing [Field adapter](../../../apps/field/field_plugin.cpp) demonstrates
lock-free meter publication and bounded UI timers; it does not supply a complete
coherent multi-field R-01 snapshot contract. That contract must be implemented
and tested if the design advances. Closed editors have no repaint timer.

Host automation should address meaningful continuous controls and explicit
actions, not bar pixel coordinates. A future adapter must document ordering
between sample-offset MIDI, automation and UI requests, then report the accepted
state. These sketches introduce no stable parameter names or state serialization.

## Evaluation before a native editor

Use the task protocol in the [interface research notes](../../../research_notes/Resonator%20harmonic%20memory/interface.md).
Success requires understanding and repeatable control, not a visually appealing
energy display. Compare both sound creation and editing effort against four
independent Sequence voices feeding Field. Stop if the memory policy cannot be
predicted, the gestures are confusing, or the sound advantage does not survive
matched-level comparison.
