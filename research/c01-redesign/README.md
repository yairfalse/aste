# C-01 musical redesign — 2026-11-17

## What was wrong

The frozen original is in `reference/`; the original production DSP remains in
`research/c01`. Baseline WAVs were actually rendered before changes. The original
is already dark: the central phrase has a roughly 133 Hz spectral centroid.
Making its low-pass darker or adding output distortion would miss the problem.
Its predominantly sinusoidal phase-modulated source lacks low-mid partial density;
its two cascaded one-pole filters have no resonant energy exchange. “Memory” is
an excitation-dependent scalar operating state, not retained audio. Short chord
patches have exactly zero output in the 12–14 second measurement window. A smooth
source, muted filtering and no audio recirculation explain why the controls can
move without creating the desired developing event. This diagnosis is structural;
“too polite” remains the creator's musical judgment.

## Controlled alternatives actually built

`circuit.cpp` is an independent C++20 engine, with four selectable lab paths:

1. Dense: band-limited partial source, narrow beating pair, independently moving
   sub component, asymmetric source shaping and signal-dependent contamination.
   Original nonresonant filtering is retained to isolate the source change.
2. Resonant: dense source into a driven two-integrator TPT resonant filter.
   Pressure changes excitation, drive and damping together.
3. Feedback: dense source with a four-line modulated, frequency-dependent,
   nonlinear feedback network. Its source cannot hear the network.
4. Full: resonant source and network interact; stored energy changes subsequent
   excitation, filter operating point and resonance. Feedback=0 removes this
   interaction, rather than merely muting an additive effect.

The fifth comparison is the frozen original. All central comparisons use exactly
the same exported MIDI events: sparse minor/suspended chords at .5, 2.75, 5.25
and 8 seconds. Separate patch triplets compare identical performances within each
triplet; the Ghost uses one chord, Mass a held chord, and Residue six repetitions.

The graph has fixed sample-based delays, not host-block delays. A bounded
Householder mixing stage exchanges return energy; low-pass erosion and high-pass
removal act inside every return. Saturation is inside the loop, not just after the
synth. Return gain is bounded below unity; integrated noise scales with existing
energy and cannot autonomously start a silent engine. Capture buffers are allocated
only during prepare, and logical clear does not erase large buffers in process.
Clear uses an output-only 5 ms fade; Panic clears immediately. Note articulation
remains sample-accurate. Feedback tails are independent of the voice release.

## Reproduce

Configure and build this directory with CMake, then run `ctest`.
`c01_baseline fresh-folder` renders the original six baseline cases.
`c01_redesign fresh-folder` renders 38 stereo float WAVs, event CSVs and levels.
`python3 analyze.py folder` adds spectral/alias measurements, hashes and HTML index.
`c01_redesign --benchmark fresh-folder` measures 12 sustained voices, 128 frames,
all main controls at maximum, 50 warm-up and 200 measured blocks per case.
Configure with `-DC01_RED_SANITIZERS=ON` for ASan/UBSan.

Local deliverables are in `build-evidence/c01-redesign/`: `baseline`, preserved
candidate iterations, `listening-final`, and `benchmark-final`. They are generated
artifacts rather than embedded copyrighted source recordings.

## Listen deliberately

Start with `listening-final/index.html`. Central dry/processed files share an
integrated loudness target, reduced as necessary for peak headroom. This controls
whole-event loudness, but long tails alter the loudness gate. Therefore the five
`foreground-*` files instead match the .5–.9 second foreground window. Compare
both sets; do not interpret a louder attack as a better circuit. Dry Full keeps
source/network interaction running but excludes the audible return.

Five targets are Submerged Chord, Corroded Stab, Feedback Ghost, Unstable Mass,
and Dub Residue. `score.hpp` is their exact parameter/phrase specification.
Ask: does the first hit have body without brittle attack? Is contamination part
of the chord? Does the tail change rather than repeat? Can Feedback and Corrosion
recover recognisable harmony? Does Residue make later events different? Are
quiet gaps compositionally useful? Reject patches that only sound bigger because
of gain, or obscure timing through excessive low-mid accumulation.

No subjective listening evaluation of these WAVs or the commercial reference
recordings is claimed. WAV rendering, comparisons and numerical checks are real;
acceptance of character, playability and reference relevance requires the creator's
listening. Metrics cannot establish that C-01 sounds extraordinary.

## References and limits of attribution

Official catalog/listening starting points: [Quadrant Dub](https://basicchannel.com/item/BC-06),
[Phylyps Trak II](https://basicchannel.com/item/BC-09),
[Radiance](https://basicchannel.com/item/BC-08),
[M-Series](https://basicchannel.com/label/M-Series), and
[Chain Reaction](https://basicchannel.com/label/Chain%2BReaction).
[Hard Wax](https://www.hardwax.live/basic-channel/) provides a legal listening route.
No protected recordings were downloaded into this repository.

In his [RBMA discussion](https://www.redbullmusicacademy.com/lectures/moritz-von-oswald-early-morning-freestyles/),
Moritz von Oswald discusses leaving space, developing a few strong elements,
versioning and decisions made while mixing. This supports a process-oriented
experiment, not a claim about particular equipment or a recipe for these records.
The requested sonic targets come from the creator's brief; audible characteristics
of individual recordings have not been independently verified in this work.

## Compatibility and promotion

The full candidate is also implemented in `apps/contamination/revised_circuit.*`.
New instances use it. All 12 existing parameter IDs, order, ranges, VST3 identity
and 16-sample latency remain unchanged. Host parameter names remain stable.
Existing state without `circuit`, or with `circuit=1`, selects the untouched classic
engine. `circuit=2` selects the new engine. Unknown revisions reject the entire
state transaction. The existing `material` property cannot identify the circuit:
old sessions already contain it. Explicit adoption preserves current parameter
values; the original four snapshots select Classic and five appended snapshots
select the new circuit. Live audio history remains unsaved, as before.

No extra sound knobs were added. BODY/PRESSURE, CORROSION, DRIFT, FEEDBACK and
EROSION/MOTION expose the existing five primary gestures. Tone and ADSR remain
visible. Stored-space feedback is measured from DSP state. This is a refined
performance layout, not a new modulation system.

## Rejected shortcuts and unresolved work

Rejected: output-only distortion, independent background hiss, generic delay
preset, another synth identity, silently revoicing old projects, and a large new
modulation matrix. Modal/chord-library expansion was unnecessary for this test.

The fixed small network is not a physically accurate room. Filter-state safety
caps are guards, not a desired sound source. The alias probe is off-harmonic
spectral energy with harmonic exclusion and finite-window leakage; it is not an
exact alias-power measurement or a promise of alias-free operation. High drive
and extreme tuning still require listening. CPU timings are local microbenchmarks,
not a guarantee against dropouts under arbitrary DAW load. Cubase musical playback
and saved-project adoption need an actual Cubase session; the suitable test host
covers loading, transport/rates/blocks, state and latency separately.
