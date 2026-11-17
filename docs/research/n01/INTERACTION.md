# N-01 interaction study

Status: design proposal and static wireframes, not an implemented editor. There
is no plugin identity, public parameter mapping or stable state contract here.
The wireframes are illustrative compositions, not captures of the offline DSP
prototype. Numeric ranges below are provisional interaction hypotheses.

## Read the screen

- [Default phrase](default.svg): an eight-quarter-note master phrase, three
  objects, their independent cycles and deliberate blank intervals.
- [Selected fragment / suspended phrase](held.svg): captured master fragment,
  its read position, held phrase position and continuing host position.

Both are 1280×760 SVGs with titles and descriptions. Grayscale is intentional:
shape, text and position must carry state before any product accent is chosen.
Background `#151719`, panel `#1d2023`, primary ink `#eeeae2`, secondary ink
`#b2b7bb`, control outlines `#777f86`. Disabled states retain explanatory text.
No decorative motion or hardware mimicry is proposed.

The master timeline is the dominant editing surface, not a miniature overview.
The master pattern governs an eight-beat phrase in this sketch; voice cycles
remain independent. A labelled VOID band gates the final output; ordinary blank
event space stops new excitations but may contain tails. These are distinct
musical operations. A destructive VOID edge is visibly marked CUT; a smoothed
edge is marked SOFT. Silence must never be represented only as a dark waveform.

Object rows show event onset, per-event repetition marks, cycle length and
phase. A bracket shows a selected audio fragment, distinct from an event box.
Filled event markers mean scheduled hits, outlined markers mean conditional
hits, and numbered small marks show repeats. Their meaning does not depend on
color. A local active repeat displays `REPEAT ×4`, while a master takeover
displays `MASTER AUDIBLE / LOCAL HIDDEN`, not a falsely stopped local playhead.

## First use and progressive detail

The first-action text is visible: “Select a mark to shape an event. Drag a span
to repeat captured audio.” Selecting a row or event never auditions audio.
Each row has a separate, labelled Audition action that also works while stopped.
An initial sparse example may help discovery, but a completely empty phrase
must remain valid and must not be auto-filled when probability changes.

Only the selected item's inspector is expanded. The fixed screen keeps master
time, local cycles, audible owner, CONTINUE/SUSPEND, RELEASE and REJOIN visible
at every scale. For an event the inspector shows count, interval, acceleration,
repeat decay, pitch/timbre change, condition and timing. A fragment instead shows
start, length, reverse, boundary shape and playback state. Values remain typed
fields; a gesture is not the only route to precision.

## Gesture meanings

| Action | Pointer interaction | Keyboard / exact alternative | Feedback |
|---|---|---|---|
| Select event | Click marker | Row focus, arrows to event, Enter selects | Outline plus “Impact event 3” |
| Add / remove event | Double-click empty row span / Delete selected | Enter in insertion mode / Delete | Explicit onset value; undoable |
| Move onset | Horizontal drag; grid snap shown | Arrow nudges by displayed grid; fine modifier uses 1/960 quarter-note; typed quarter-note or millisecond offset | Original ghost plus exact delta; do not mutate neighbours |
| Repeat event | Drag repetition handle | Typed count and interval | Numbered repeat marks and final onset; invalid overlap is not silently truncated |
| Select audio | Drag bracket endpoints | Focus Start/Length; type sample or millisecond values | Source label, available-history extent and captured length |
| Local repeat | Press row Repeat | Focus action, Enter toggles | State text and local cursor; other rows continue |
| Master hold | Press and hold Hold; separate latch checkbox | Focus Hold, Space down/up; latch via Enter | Active source, time mode and return destination |
| Release | Release momentary Hold or press Release | Enter on Release | Defined release boundary and resulting phrase position |
| Rejoin | Explicit Rejoin action | Enter on Rejoin | “Rejoin at next master boundary”; cancel remains available |
| Structural silence | Draw VOID span in master lane | Insert Void, type start/length and CUT/SOFT edge | Labelled span; final-output precedence visible |

No essential control depends on double-click, dragging or simultaneous keys.
Space is consumed only by a focused momentary action, never as a global editor
transport shortcut. Exact key routing must be tested in Cubase before adopting
shortcuts. Escape cancels the current uncommitted edit and restores its original
value; it does not release unrelated audio holds. Undo operates on committed
edits, not on every pointer movement. Key auto-repeat never emits extra Hold
edges. Losing focus during a momentary hold emits a release request; latched
holds remain latched and visibly labelled.

## Time and nested feedback

CONTINUE means underlying phrase time advances while the captured fragment
owns the output. RELEASE returns to the currently reached phrase position, not
the selection's start. SUSPEND means phrase time, source sound state and local
fragment state are held while host time and master-fragment playback keep
moving. RELEASE resumes that held state at the current tempo; REJOIN is a
separate, explicitly scheduled jump to host-aligned phrase time that invalidates
temporal audio history. The prototype exercises immediate Rejoin; the UI's
next-boundary Rejoin is a proposed quantization option, not implemented behavior.

Do not label either mode “sync off.” Host synchronization and a phrase offset
can coexist. Label three positions in the held view: HOST, PHRASE and FRAGMENT.
When aligned, the first two may share a line but retain a readable combined
label. A seek invalidates capture according to the DSP's declared policy and
shows “Transport moved; capture unavailable” rather than a fabricated waveform.
Missing host timing is UNKNOWN, not “120 BPM / beat 1.”

A simultaneous master operation wins audibility over a voice operation, which
wins over its source event. In CONTINUE, hidden source/local state advances; in
SUSPEND it freezes. The UI must report the accepted state; the choice belongs to
the audio model, never the paint code. A VOID can override
all audible layers. If both a gesture and automation target one semantic action
at the same timestamp, the adapter must apply one documented order and display
the accepted result; host automation cannot be silently overwritten by a stale
inspector value.

## Repeatable performance capture

Record Gesture is a visible arm state, distinct from audio capture. The proposed
recording is a bounded semantic event list containing phrase-relative timestamp,
target object/fragment identity, action, value and insertion order. Record Hold
press and release edges, mode, selection endpoints and deliberate cuts—not
30 Hz UI snapshots. Replaying the same event list and seed must reproduce the
same accepted actions independently of editor refresh rate and host blocks.

For the first experiment, cap the list at 256 actions per phrase. At capacity,
stop recording new actions and show `GESTURE FULL / 256`; do not truncate an
existing release edge into a permanently stuck hold. Reserve or pair space for
momentary releases. Edits are staged off-thread and transferred via a bounded
audio-owned command boundary; queue overflow is reported off-thread. A replayed
gesture cannot recursively record itself. Re-arm is explicit. Capture/replay and
Cubase automation are alternatives to compare, not two unowned writers.

No stable host IDs are proposed. Continuous macro automation needs meaningful
physical units and begin/end gestures when a future adapter exists. Structural
event lists require a versioned state document, not hundreds of accidental
scalar parameters. The candidate ownership split is host automation for a small
set of performance values, phrase state for events and captured gesture actions.

## Keyboard and accessibility

Tab order: master timing/status → time mode → master selection → Hold/Release/
Rejoin → Void → each object's row/Audition/cycle/local repeat → selected inspector
→ gesture record/replay → output. Within a timeline use one roving focus target
and arrow navigation, rather than hundreds of Tab stops. Focus follows stable
object/event identity after edits and returns to the invoker when detail closes.

Every control needs accessible name, role, value/unit, state and help text.
Example: “Pressure event 2, onset 1.25 quarter notes, four repeats, conditional
every second cycle.” Read-only position status must not masquerade as a slider.
Announce user-triggered state changes, overflow and unavailable captures; do not
announce every audio sample, beat or meter update. Offer a low-motion display
that keeps numeric positions and static state labels. Focus is a high-contrast
two-pixel outline; selection is an inset fill, so the two remain distinguishable.

Scale between proposed 1024×680 and 1800×1060, with minimum 14-pixel body type
and 32×32 logical-pixel control targets; provide a larger 44-pixel-target mode
if touch is pursued. Expand lane spacing before enlarging decoration. At minimum
size retain all three rows and essential master actions; the inspector may use
two columns but no hidden page. Visual zoom and time zoom must be separate.
Text entry validates finite values and explains bounds inline, preserving the
last valid value. Never silently turn missing or malformed data into zero.

## Evaluation checklist and limits

The SVGs do not accept input, play sound, record automation or expose native
accessibility controls. Their geometry and XML can be inspected; their behavior
cannot pass interaction tests. No human preference or musical success is claimed.
Both files passed `xmllint --noout` and were visually inspected as full-width
Quick Look PNG renders on 2026-11-14. Quick Look cropped the initial landscape
thumbnails, so temporary square-padded copies were used for inspection; padding
is not part of the delivered SVGs. This is visual layout evidence only.

Before a real editor advances:

1. Ask new users to locate a VOID, create four accelerating repeats, and predict
   the return position in both time modes. Record errors, completion time and
   confidence before explaining the design.
2. Repeat the tasks keyboard-only and with VoiceOver in Cubase. Check focus
   routing, numeric entry, the host transport shortcut and momentary focus loss.
3. Test 1×/2× scaling, minimum bounds, long translations, exact values and a
   high-contrast setting. Measure text and control contrast rather than infer it
   from screenshots.
4. Record a gesture once, replay under different buffer partitions and editor
   frame rates, and compare accepted action logs and audio. Stress full queues,
   duplicate actions, seek, stop/start and automation conflicts.
5. Compare the same musical task against Impulse and Impulse → Loop, including
   effort to prepare it and predictability of its return. Preference is a human
   response, not a peak/RMS result.

Sources: [Aste UI system](../../UI_SYSTEM.md),
[Impulse contract](../../products/impulse/PARAMETERS.md),
[Loop contract](../../products/loop/PARAMETERS.md),
[listening protocol](../../testing/LISTENING.md). These wireframes are derived
from repository evidence and the brief, not from reproduced hardware panels.

## Cycle-2 articulation inspector study

The selected-event detail region can compare Restart, Carry and Crossfade as
three provisional alternatives. Describe their audible job rather than hiding
it behind a generic Smooth control. The lab Crossfade has a fixed 2 ms duration
and one outgoing tail; overlapping shorter repeats can replace that tail. Do
not display an unqualified click-free guarantee. Carry preserves source phase
and age but renews amplitude and locks, so it is not simply a legato envelope.
Keep the repeat-onset marks fixed when changing articulation. Exact numerical
entry and keyboard selection must remain available. This is an inspector study,
not a new stable parameter set or implemented native widget.
