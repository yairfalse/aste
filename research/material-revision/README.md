# Musical-intent revision — 0.8.0 development

This implements the musician’s [artistic brief](../../docs/MUSICAL_INTENT.md) after
[the previous audit](../intent-review/README.md). It is a working musical
revision, not a claim that the musician has accepted the sound.

## Implemented decisions

| Instrument | Audible/interaction change | Deliberate limit |
|---|---|---|
| Density | Weight / Color introduces input-linked low body, upward lift and slower recovery before the existing nonlinear shaper | No autonomous bass oscillator; no replacement compressor topology |
| Harmonic | Nonlinear resonant participation remains available in flat and cut regions; stronger excitation makes Input/PUSH/PULL consequential | Harmonic at zero remains the linear EQ; no new modulation matrix |
| Sequence | Wheel/CC1 and channel pressure add expressive Pressure; CC74 moves brightness with smoothing and sample-offset handling | No vintage circuit claim; existing MIDI notes, polyphony and articulation remain |
| Loop | Evolving material starts with a recorded/printable take and stronger generational wear | Its existing print/history engine already implements inheritance; no redundant new DSP |
| Impulse | Each invented object has a three-mode excited body whose energy survives source retriggers and affects subsequent behavior | Existing eight objects and rhythm model retained |
| Field | Energy-dependent movement, orthogonal return rotations and energy-balanced FOREVER; spatial controls precede optional phrase capture | Deliberately evolving hold, not immutable freeze; bounded correction and saturation can dissipate energy |
| Non-Machine | One base sound per object; explicit independent hit locks; separate Pattern/MIDI input; isolated audition and visible sound owner; restrained authored starting score | Three objects, eight events each; automation enabling a lock must also provide its intended sound values |
| Resonator | Stored energy deforms modal tuning and decay; root/structure edits retune existing energy without requiring another strike | Modal product engine only; waveguide remains a research alternative |

The same excited-body helper is reused in Impulse and N-01. Their time models
remain independent. N-01 audition temporarily crossfades from the main engine;
the main phrase continues, so returning from audition does not restart it.

## Recall and parameter identity

Existing parameter IDs and ordering remain intact. N-01 appends 21 sound-lock
booleans at host slots 424–444; its old 424 slots retain their identities. Other
products add no performance parameters. The root state attribute `material`
selects the response, independently of earlier `behavior` revisions. Missing
means previous sound, `0` means previous, `1` means updated; other values reject
the state transaction. New instances default to updated sound. Opening an editor
does not adopt it. USE UPDATED SOUND preserves parameter values and saves the
choice. Selecting a new material scene in N-01, R-01 or Field is an explicit
adoption. Loop’s change is a new preset, not a DSP revision.

## Reproduce

The `reference` directory contains the actual pre-edit 0.7.1 DSP snapshot from
this working tree, with SHA-256 manifest. It is not substituted from HEAD.
Configuration validates every source before making a namespace-isolated copy.

```sh
cmake -S research/material-revision -B build-material-review -DCMAKE_BUILD_TYPE=Release
cmake --build build-material-review --parallel 6
build-material-review/material_review build-evidence/material-revision/fresh-listening
cmake -S research/material-revision -B build-material-sanitized -DMATERIAL_SANITIZERS=ON
cmake --build build-material-sanitized --parallel 3
build-material-sanitized/material_review
```

Use a fresh output directory. On this laptop, builds require the SDK C++ headers
specified in the main build notes. Sanitized runs are correctness evidence, not
performance measurements.

## Listening pack

The lab writes 36 stereo float WAVs at 48 kHz, eight seconds each, together with
`measurements.csv`. Effects receive three paired previous/material renders with transient, chord
and mixed sources (`0`, `4`, `5`). Impulse receives three different objects.
Sequence/N-01/Resonator each have one authored paired stimulus (`5`); these are
behavior demonstrations, not comprehensive preset surveys. Loop’s comparison changes curated
process settings; all other pairs compare response revisions.

Each pair is gated-loudness matched within 0.05 LU, at -23 LUFS or a lower common
target to keep peak below 0.8. Raw peak/RMS remain in the CSV. Loudness matching
avoids accepting “louder” as “better.” No claim of musical listening assessment
is made by the automated lab. Audition in Cubase must still assess physicality,
control, fatigue, usefulness of silence, and whether history is perceptible.

## Correctness evidence

The lab checks exact previous-response samples against the frozen implementation
for all eight products at 44.1/48/88.2/96/176.4/192 kHz (48 comparisons), finite
updated output, and exact 127/511-frame partition equality. Causal tests check
excited-body tails/reset at all six rates, moving Field holds and their release,
Sequence expression onset and old-response isolation, and non-additive Resonator
memory. Adapter tests check transactional state, adoption, stable recall, output,
MIDI, bounded processing and real-time audits, plus N-01 lock initialization,
Pattern/MIDI ownership and isolated audition status.

A repeated impulse test measures the residual between two excitations together
and the sum of separate excitations. Previous R-01 is effectively linear
(-146.5 dB residual); the material response gives -1.73 dB with the selected
energy-dependent settings. That proves interaction, not desirable timbre.

## Remaining evaluation

The CPU printout is end-to-end offline wall time including preparation/fixtures,
not an audio-callback deadline distribution or a Cubase multicore benchmark.
Feedback is bounded, but extreme automation and modulation can deliberately
change spectra. This revision does not establish an alias-free claim or replace
full release listening/accessibility testing. Captured/live energy is still not
saved as audio in sessions. Universal plugin validation and local installation
are separate from the offline lab; see the [verification record](EVIDENCE.md).
