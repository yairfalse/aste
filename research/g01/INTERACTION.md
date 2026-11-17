# Interaction study and proposed product behavior

The [HTML prototype](interface.html) implements routing/selection controls and
replays recorded telemetry. It does not process audio. The study uses the Aste
matte background, technical text, restrained accent and visible task hierarchy;
there are no decorative patch cables or hardware controls.

## Implemented prototype interactions

- Three visible nodes; select in graph or with explicit A/B/C buttons. Keyboard
  Enter/Space selects a focused graph node. Selection and inspector stay linked.
- Directed graph and numeric matrix share one `model.routes[destination][source]`.
  Connection selectors create a route at 0.5; matrix fields set exact 0–1.5
  values or zero to disconnect. The graph shows direction, self-loops and strength.
- Rows show effective feedback weights after the 0.995 row ceiling. The graph
  line width represents requested strength; the matrix reports effective values.
- Routing scenes preserve node sounds. This mirrors the core’s routing-only
  morph decision; there is no hidden midpoint change of pitch or material.
- Selected-node inspector keeps Size/Density/Speed/Pitch, fragment selection,
  time/stereo scatter, width/balance/drift, output level, capture size,
  accumulation, erosion and inheritance together. Exact numeric entry accompanies
  every slider. Reverse, freeze, capture, window and time mode are explicit.
- Four global candidate controls are visible at the top. Freeze-all does not
  overwrite local freeze flags. Clear is explicitly a request in a non-audio
  prototype; it never pretends to erase real DSP memory.
- Mutate changes selected position using a seeded local generator. This tests a
  gesture and does not claim identical randomness to the DSP action stream.
- A file picker loads actual `telemetry.csv`. The readout shows recorded second,
  active grain count, stored/output RMS, capture edition count and frozen flag.
  Editing graph controls cannot change recorded telemetry. No synthetic energy
  animation or playback simulation is shown.
- JSON export preserves the UI study model, explicitly distinct from `G01LAB 1`
  DSP controls. It is not a fake plugin/session preset.

## Proposed native performance screen

Keep the graph and matrix beside one selected-node inspector rather than
permanently exposing three full parameter panels. Always show each node’s
capture/freeze state, current grain count and stored energy, with bank-pressure
and dropped-birth indicators. The lab has those counters; the HTML readout does
not yet expose all of them. The initial screen must explain that node Output is
a monitor level and routes are pre-fader. Final Master and Mix belong in a fixed
header. These are product requirements, not an implemented native editor.

Freeze seals a partial capture and then preserves it. Clear discards only the
selected node’s memory/grains with a short outgoing transition; another node
can refill it through routes. A separate global reset should explicitly clear
the whole network. Do not label node Clear as a global panic. Capture stalls
must say “waiting for grains to release source banks,” rather than showing a
moving recorder that is not writing. A source with no material must say empty,
not merely display zero output. Requested density and actual dropped births
must be distinguishable.

All controls need stable titles, exact text entry, fine adjustment/reset,
visible focus and meaningful inactive reasons. Matrix editing must be fully
keyboard-accessible; graph dragging, if added, cannot be the only route action.
At smaller widths, selected detail may reorganize while critical state and
freeze/clear remain visible. The responsive HTML is exploratory; it has not
passed real-browser zoom, minimum-screen, touch or screen-reader testing.

## Evaluation still required

Observe a musician creating a three-node cycle, selecting the audible source,
freezing one memory while recapturing another, editing pitch without changing
speed, and returning to a routing scene. Record misunderstandings and time to
first controlled result. Test whether routing morph and four macros are easier
than direct matrix performance. Confirm that energy telemetry helps a musical
decision and that silence is legible. No usability session or native accessibility
approval has been performed in this cycle.
