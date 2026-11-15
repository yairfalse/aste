# Play the six instruments in Cubase

Use the Apple-silicon test bundles, in a copy of a project. Nothing in this pack
installs itself. These are ad-hoc-signed internal builds, not a notarized release.
Do not bypass macOS security checks; report a rejected bundle if one occurs.
Keep the original project because older binaries cannot read newer state schemas.

For a recalled sound, first inspect the response button. USE CURRENT RESPONSE
is an explicit upgrade that retains knobs, patterns and Loop tapes. Save the
test copy to keep that choice. A matching preset name describes parameter values,
not the response version. CURRENT RESPONSE is disabled because it is already active.

Start with the matched files in `audio/`, not `raw/`; keep monitoring level fixed.
The comparisons are named, not blinded, and contain generated diagnostic material.

1. **Density:** in `open-starts/density/audio`, compare percussion and bass
   `previous_start` (Grip) with `open_start` (Pulse / parallel). In the plugin,
   move Blend between 40–100%, then Drive, then trim Output. Can you deliberately
   get pressure without losing the attack? The goal is not the loudest setting.
2. **Harmonic:** in `open-starts/harmonic/audio`, compare chord `previous_start`
   with `open_start`, then `open_push_3dB` and `open_pull_3dB`. Start Color / open
   in the plugin; boost a band and use PUSH/PULL. Is contour versus coloration
   understandable? The live buttons are paired gains, not automatic loudness matching.
3. **Sequence:** compare phrase `same_controls_legacy` and `after_start` in
   `six-product-comparisons/sequence/audio`. Focus a step and use G/A/S; play
   separated and overlapping MIDI notes. Does articulation respond predictably?
   Text fields keep note entry; Command/Control combinations are not consumed.
4. **Loop:** compare generated-music `previous_start` and `open_start` in
   `open-starts/loop/audio`. Select First take, feed audio, capture, stop capture,
   transform, RELOOP, then revisit earlier generations. Save and reopen a tape.
   At Overdub 100%, no new input is written; that is a retention endpoint.
5. **Impulse:** use `click-sub/` rather than the historical Space comparisons.
   Start Clicks / sub. Place one click and one sub deliberately; select silently,
   AUDITION explicitly. Try ratchet 3, then move the host buffer size. SHOW ALL
   OBJECTS adds sounds; focus returns space to the two objects you are using.
6. **Field:** compare chord/sparse `same_controls_legacy` and `after_start` in
   `six-product-comparisons/field/audio`. Excite, hold, change Grain/Pitch/Motion,
   release. Print a held performance to audio before closing: session state
   recalls the controls, not the live held sound.

For each, record: source/project; build; chosen response; task; preferred version
or no preference; what became easier; what became worse. Include host buffer size
and sample rate for timing issues. Passing these tasks requires your musical
judgment; a successful render or higher meter reading is not acceptance.
