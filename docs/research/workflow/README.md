# Family workflow revision — 0.7.1

Subsequent musician feedback rejects Non-Machine’s controllability and finds Resonator musically insufficient. The technical checks below remain valid; this revision is not musical acceptance. See the [intent reassessment](../../../research/intent-review/README.md) and [artistic brief](../../MUSICAL_INTENT.md).

The musician reports that important abilities are hidden and the family is hard
to control. This working revision addresses observed interface mechanisms, not
just control counts. It does not claim a completed usability study or musical
acceptance. The DSP algorithms and host parameter contracts stay as in 0.7.0.

## Task audit and implemented changes

| Instrument | Concrete obstacle | Change |
|---|---|---|
| Density | Character was a small control under meters, while Density dominated; two output controls were unexplained | Character and Density share the main macro column. Guide connects input, shaping, Blend and final Master; Output is described as processing trim |
| Harmonic | Color can appear ineffective with flat/cut bands; level gestures are separate from band shaping | Visible guide explains the dependency; contextual help reports whether boosting/Harmonic level is missing. PUSH/PULL stays visible |
| Sequence | Sequence reads as playback although DAW transport owns it; sound editing and step editing are disconnected | Visible toggle says DAW PATTERN, mode status identifies a stopped DAW, and the guide connects MIDI/pattern, voice, filter, envelope and steps |
| Loop | RELOOP and generations hide the ordinary record/print/compare workflow; inactive time controls lack visible reasons | PRINT NEW TAKE, OLDER TAKE and NEWER TAKE describe the actions. Guide exposes capture/play/transform/print; inactive time and print controls explain the blocker |
| Impulse | Empty objects and eight cycle controls are hidden at startup | All eight objects start visible. Selected sound's five sound and eight rhythm controls, plus Generate and Audition, stay visible together. Focus Active is optional |
| Field | The large FOREVER control outranks actual phrase capture; toggles keep their capture/repeat labels even when release is the action | Capture/inner repeat panel comes first. Smaller HOLD SPATIAL TAIL is separate below; capture and inner buttons say RELEASE while active |
| Non-Machine | Material changes affect the object, but pitch/decay/color edits affect only the selected hit; applying a sound across the object is laborious | Scope is visible, and USE HIT SOUND FOR ALL HITS copies only five sound locks while preserving the pattern. Guide explains holds masking edits |
| Resonator | Excitation, tuning and retained energy appear in an unrelated grid; ineffective Partial Retain looks usable | Three named task groups pair with Excite, Retune and Clear. Partial Retain is inactive with a reason outside Partial mode; held individual MIDI tones explicitly own tuning. Root supports note names |

The family now uses a visible task path and a 48-pixel control-help strip.
Pointer and keyboard focus show control meaning in the panel, rather than
requiring tooltip discovery. Inactive-parent explanations override generic
child help and refresh as conditions change. Guide updates are UI-only, bounded
at 12 Hz, and never load a preset, audition, or write a parameter. The text does
not replace the actual action controls or become a hidden tutorial page.

Two incorrect shared descriptions were found and corrected: Field Pitch is
fifth/octave return amount, not Loop's independent pitch transposition; Resonator
Mutation is spectral variation in cents, not Impulse's rhythmic mutation.
Experimental source/timing controls now have product-specific meanings.

Non-Machine and Resonator use the same preset identification behavior as the
other six: match the actual parameter snapshot, retain a name plus an asterisk
after edits, and identify unmatched recalled sounds as CUSTOM. Reference scene
values are made in unattached temporary instances on the UI thread; the live
instrument is never changed just to identify its starting point.

Historical sound adoption now says USE UPDATED SOUND / UPDATED SOUND. The
existing compatibility opt-in and serialized metadata are unchanged. Opening an
editor still retains the saved sound automatically. Master remains in the same
header position across all eight products.

## Interaction checks

Tests cover guide visibility and no state changes when opening/reading it,
current inactive explanations taking precedence, all Impulse objects and cycle
controls available by default, optional focus views remaining non-musical, and
Non-Machine's apply-sound action preserving event placement. Existing parameter,
state, callback, editor-size and VST3 host tests remain required.

Native minimum-size screenshots are generated through the existing editor test
targets. Component checks and screenshots establish placement and behavior,
not live Cubase, VoiceOver or preference approval. No new allocation, lock or
file operation was added to audio processing. Resonator publishes a bounded
MIDI-ownership status flag so the interface does not falsely disable its fallback
chord controls when no individual tones are held.

## Next user tasks

Without verbal coaching, try: make/hear a first sound; locate its strongest
shaping controls; build or capture a short phrase; perform one change; release
or return predictably; match output level. Record the exact instrument/action
where the screen fails to communicate what owns the sound or time.

This is a first family workflow pass. Pattern undo, a consistent editor-selection
recall policy, task-based preset curation, and real native accessibility testing
remain separate work. Extra explanatory text cannot resolve every sound-design
or timing-model problem; further changes should follow the next concrete task
failure rather than add more controls by default.

## Completed local verification

All 21 targeted adapter/editor/VST3 checks passed, with Harmonic's artifact
export also run directly. All eight minimum-size editors were rendered and
visually inspected. Non-Machine and Impulse selected-detail controls use wider
linear sliders with exact fields rather than miniature rotary targets. The
Resonator test checks note-name and numeric root entry after its host attachment.

All eight universal VST3 0.7.1 bundles were installed together after Cubase was
closed, with the previous bundles backed up. Each installed bundle then passed
the actual VST3 ABI smoke host with the expected parameter contract, finite
audio and zero reported latency. This is local installation and regression
evidence, not a completed Cubase or human usability study.
