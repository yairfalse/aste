# Changelog

## 0.12.0 — Field reverb foundation

Promote the evaluated diffused 16-line Hadamard reverb to new Field instances. Calibrate per-line decay and wet gain; separate fragment/pitch readouts from feedback losses; retain stored energy in FOREVER. Add six factory starting points, explicit engine adoption, byte-stable state and old-engine sample-exact regression coverage. Preserve all 19 parameter identities and the nine previous snapshots. Validate six-rate DSP/real-time behavior, sanitizers, native/Intel VST3 automation and recall.

## 0.11.0 — C-01 interacting harmonic memory

Add a dense harmonic source, nonlinear resonant filtering and bounded degrading audio feedback with space-to-source interaction. New instances use the revised circuit and five new sound targets. Older sessions retain the original engine until explicit adoption; all 12 parameter identities and the fixed 16-sample latency remain unchanged. Preserve the host-restart fix. Add original/candidate renders, matched listening comparisons, real-time and compatibility tests, sanitizer checks and measured performance evidence under `research/c01-redesign`. Musical acceptance remains a listening decision.

## 0.10.1 — C-01 host restart fix

Preserve the product-declared latency during preparation and declare C-01’s fixed 16-sample latency at construction. This removes the repeated 16→0→16 host restart cycle. Add bounded host-reprepare regression coverage, rate/block-size checks and a zero-latency base probe. Sound parameters and DSP are unchanged.

## 0.10.0 — Contamination C-01

Add the existing Contamination C-01 MIDI instrument to the normal universal VST3 build and local installer. Preserve its 12 parameters, state identity and DSP. Validate its intentional 16-sample latency in the suitable host.

## 0.9.0 — Grain G-01

Grain adds a mono/stereo VST3 effect with three granular memories, an open directed routing matrix and graph, independent node timing/pitch, freeze, erosion, route-only scenes/morph, and final Master Output. Sinc quality uses measured rate-dependent voice caps. Control state recalls; captured audio remains live memory and must be rendered before closing. The original laboratory remains independent.

## 0.8.0 — 2026-11-16 — Material response (development)

- Density adds input-linked body and recovery through Weight / Color. Harmonic transforms flat and cut regions as well as boosts. Sequence exposes wheel Pressure, aftertouch and CC74 brightness.
- Impulse bodies retain energy between strikes. Field rotates and balances its grain/pitch feedback under Forever; Living field starts with the evolving space exposed. Loop starts with an editable generational take.
- Non-Machine separates object sounds from explicit per-hit sound locks, separates Pattern/MIDI ownership, and provides isolated audition. Its 21 new sound-lock parameters append after the existing 424 slots. Resonator adds energy-dependent modal deformation and decay, plus retuning of retained chords.
- Old sessions retain the prior response. USE UPDATED SOUND adopts the new DSP while preserving control values; new instances use it by default. Comparison WAVs and causal tests are evidence of behavior, not musical acceptance. See [material review](research/material-revision/README.md).

## 0.7.1

Family workflow: visible contextual control help and task paths in all eight editors; expose Impulse objects and rhythm controls; promote Field capture; group Resonator tasks; clarify Non-Machine hit/object sound scope; name Loop print/take actions; make pattern ownership explicit in Sequence. Preserve parameter contracts and saved sound adoption.

## 0.7.0

Non-Machine adds selectable object materials, direct event timelines and an empty-pattern action. Field adds stereo phrase capture with nested fragment repetition, reverse, resizing, speed and deterministic release to the advancing outer phrase. New controls append and older states retain prior audio paths.

## 0.6.2

Density adds Character and Bite / breathing; Field adds Focus and Moving glass for bass-controlled, brighter moving pitch/grain returns. Both new parameters append after existing host parameters and restore zero in older sessions.

## 0.6.1

Non-Machine: prevent overlong repeats from freezing pattern edits; distinct scene rhythms, seeded NEW RHYTHM action, visible positions and capture feedback.

Versions apply to the complete six-instrument family. See
[versioning](docs/VERSIONING.md) for numbering and compatibility rules.

## 0.6.0 — 2026-11-15 — Playable Non-Machine and Resonator (development)

- Non-Machine N-01 and Resonator R-01 now have separate C++20 product forks,
  JUCE boundaries, native single-screen editors, MIDI, explicit final Master
  Output, project-control recall and universal VST3 targets. Research labs and
  historical listening packs remain independent.
- N-01 exposes three objects, 24 event slots with locks/repeats, local cycles and
  capture, master Continue/Suspend capture, reverse/shorten/release/rejoin, and
  structural void. MIDI/HIT audition works while stopped; DAW BPM/PPQ drive its
  sequence. Invalid repeat trains report that the last valid pattern is sounding.
- R-01 exposes modal harmonic memory, five exciters, root-chord and four-tone
  keyboard modes, sustain, explicit excitation/retune/clear, and actual bank
  energy. Clear/restore empties live energy without discarding the next MIDI note.
- Controls save with schema 1; N-01 captured audio and R-01 sounding energy do
  not. Distinct plugin identities and parameter IDs avoid altering old products.
  Live Cubase listening/accessibility approval remains open; these are playable
  experiments, not claims of a finished musical identity or hardware emulation.
- The local eight-bundle installer stages and validates before replacement,
  preserves a rollback set, and leaves projects/settings intact. The original
  six-product native .pkg scope is unchanged.

## 0.5.0 — 2026-11-15 — Master level, Field texture and Sequence control (development)

- All six plugins add a visible header Master Output: -36 to +18 dB, smoothed
  after sound shaping. Existing Output/drive behavior and old automation indices
  remain unchanged. Output meters include this final stage. Old states without
  Master restore unity; fresh Impulse starts at +6 dB and Sequence at +3 dB.
- Knobs have slower drag sensitivity and Shift-drag fine adjustment with typed
  values. Sequence adds independent filter attack, decay and sustain; zero
  attack/decay follows the amplifier. New-voice waveform blends align oscillator
  polarity to prevent the saw/pulse midpoint losing fundamental/body. Original
  mono is unchanged; 0.4.0 Poly/Mono sounds change with this correction.
- Field adds Texture: exposed overlapping grains and an audible stereo fifth/
  octave wet layer, with reduced direct feedback as pitch return rises. Fresh
  instances start on the more active Open Field. Old states restore Texture 0.
- Impulse identifies as Instrument/Drum rather than Instrument/Synth. This is
  still one drum instrument containing eight objects, not eight separate plugins.
- Musical approval is pending hands-on Cubase listening; no hardware-fidelity
  claim is made. Previous builds remain the route to exact old Poly/Mono renders.

## 0.4.0 — 2026-11-15 — Polyphonic Sequence (development)

- Sequence adds eight-voice polyphonic keyboard playing, a new warm dual-oscillator
  voice, independent filter contours, per-voice stereo placement, deterministic
  tuning/PWM motion, sustain pedal and bounded voice stealing.
- Fresh instances open on Warm Brass with the sequencer off. Wide Strings and
  Slow Glass offer sustained sounds; Pulse Bass and Acid Current run the mono
  sequencer with accents and slides. Original Mono remains available.
- Schema 4 appends Voice Mode after all 85 existing parameters. Schemas 1–3
  restore the original mono engine and their previous timing behavior. Existing
  plugin identities and automation indices remain stable.
- OB-8-inspired musical direction; no hardware-emulation or listening approval
  claim. Other five instruments have no DSP changes.

## 0.3.0 — 2026-10-03 — Decisive tone and dynamics (development)

- Harmonic Foundation is now a low shelf and Air a high shelf: gain changes
  cover the whole bass/top end, not only a bell around the selected frequency.
  Body and Presence remain broad bells. Shelf frequency marks half gain; gain
  labels describe the shelf plateau. Updated tooltips explain the controls.
- Density starts at 90% processed Blend with -7 dB Output. All five existing
  presets have more processed sound and compensating Output trims. Threshold,
  ratio and saturation are not pushed further; the dynamics are less diluted.
- **Audible compatibility change:** existing Harmonic Foundation/Air settings
  render as shelves. Existing Density parameter values still render as before;
  its stronger blend applies to new instances and explicitly loaded presets.
  Parameter IDs, ranges, schemas, neutral EQ, bypass and protection are retained.
  Keep 0.2.0 for exact previous Harmonic renders. Listening approval is pending.
- Local verification: all 70 regression checks, 537 Steinberg checks per changed
  plugin, DSP ASan/UBSan, static analysis, formatting and CPU budgets passed.

## 0.2.0 — 2026-10-02 — Track-level impact (development)

- Density engages quieter tracks: the threshold mapping is 16 dB lower, with
  additional crushed-path make-up, a 75% processed default blend, and -6 dB
  default Output trim to avoid presenting increased loudness as the improvement.
- Harmonic boosts shape wider frequency regions without exaggerating the dB
  labels. Five coloured presets now provide stronger tonal contrasts. Zero
  gain, the neutral Default preset, and clean cuts are preserved.
- Fix Harmonic coefficient smoothing stopping short of low-frequency gain
  targets after knob changes; use double-precision EQ coefficients and state.
- **Audible compatibility change:** saved parameter values, IDs, ranges and
  state schemas are unchanged, but Density and boosted Harmonic bands sound
  different. Keep 0.1.1 for exact old renders. Saved Blend values are respected;
  only new instances and the Default preset start at 75% Blend/-6 dB Output.
- These are measured development changes, not listening-approved release
  candidates. Level-match with Output when comparing in a DAW.
- Local verification: 70 regression checks, 537 Steinberg validator checks per
  changed plugin, DSP ASan/UBSan tests, static analysis and formatting passed.

## 0.1.1 — 2026-10-02 — Development update

- Add an opt-in cleanup utility that previews older per-user Aste AU/VST3 copies
  and moves confirmed matches to a dated backup outside plugin scan folders.
  It requires a newer verified system replacement and leaves same/newer,
  unknown, symlinked and unrelated bundles alone. No permanent deletion.

- Establish a shared release history and version-consistency checks for AU,
  VST3 and installer components.
- Remove a fixed version from the CI package inspection path.
- Keep the original 0.1.0 download unchanged. This remains an unnotarized local
  development build, not a tagged stable release.
- Local verification: 70 checks passed, including isolated cleanup fixtures
  covering version/identity checks, symlinks, changed previews and no-overwrite
  moves. No installed plugin was moved during development.

## 0.1.0 — 2026-10-02 — Development baseline

- First locally shared all-six-instrument macOS installer: universal Intel and
  Apple Silicon AU/VST3 bundles, selectable by instrument and format.
- Calmer Impulse defaults and direct drum audition; clearer interactions across
  all six instruments. Loop saves captured audio and retained generations.
- AGPL-3.0-only first-party licensing, with matching Aste and JUCE source included.
- Local evidence: 69 checks passed; extracted plugin bundles and the disk image
  were verified. Source configured without downloading JUCE.
- Not notarized. Intel execution, oldest-macOS and real-DAW compatibility remain
  unverified for this build. It is not a supported stable release.

The older internal prototypes also used 0.1.0. This entry identifies the
`Aste-0.1.0-macOS-universal-development.dmg` baseline, not every historical build
with that embedded number. The original download is preserved unchanged.

Baseline DMG SHA-256:
`9ea34b188d574b85a91dd78add4050f88f7d0d792e4d091d8ad0d03e7f0e0af4`.
