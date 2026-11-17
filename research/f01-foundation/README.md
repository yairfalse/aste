# Field F-01: conventional-reverb foundation investigation

2026-11-17. Production Field code, parameter/state contracts and installed plugin
are unchanged. This is an isolated C++20 offline lab, not a replacement reverb.
The creator's adverse Cubase listening result is the motivating evidence.

## Diagnosis of the real implementation

`reference/` freezes `apps/field/field_processor.*` and `nested_memory.hpp`, with
SHA256 hashes. The product adapter and preset/state paths were also inspected.

The current foundation is eight 31.1–82.1 ms fractional delays, a Householder
reflection, one-pole feedback damping and signed stereo taps. Input goes directly
to the long lines. There is no input allpass diffusion or independent early/late
structure. With ordinary textures disabled, the first arrivals are isolated long
line taps rather than a progressively diffuse onset. Measured density supports
this diagnosis; metallic ringing or objectionable flutter still require hearing.

All lines share `retention`, despite different traversal times. Consequently,
individual paths have different implied decay times. Mass changes this gain
rather than a calibrated RT60. Its 0.70 lower limit also prevents the short
nominal 0.6 s setting used in the lab: minimum Mass was reached, yet measured
broadband T20 extrapolates to about 1.0 s. This is a real control limitation.

Distance simultaneously darkens input and every feedback return. At the nominal
2.4 s baseline, measured 125 Hz / 8 kHz decay is about 2.55 / 0.99 s. This is not
inherently wrong absorption, but a single tonal gesture obscures decay control.
Tanh is applied on every write, making decay level dependent. Equal-power wet/dry
mixing is not an energy calibration; Texture makeup and pitch-layer gain further
change loudness. Stereo sign patterns alone are not proof of depth.

Crucially, Grain/Pitch/Motion are part of the feedback system rather than cleanly
optional decoration. Motion=0 still has a 0.4 ms sinusoidal delay excursion.
Grain shifts read heads by up to 14 ms times its amount, with random updates and
12 ms smoothing. Fractional interpolation changes high-frequency energy. Pitch
reduces ordinary loop gain while adding a differently distributed return.
The default-feature case measures about 0.73 s broadband decay at the same Mass
where the neutral case measures 2.13 s. The transformation path can therefore
remove much of the body it is expected to preserve. This is a measured association
across a parameter bundle, not attribution to one specific term in isolation.

FOREVER's return-energy correction and later loop-capture additions do not solve
the onset/diffusion question. They were not expanded or promoted in this work.
An FDN is not inherently unsuitable; this particular excitation, decay and
readout design has quality limitations worth replacing, rather than defending.

## Competing engines actually implemented

| Path | Topology and purpose |
|---|---|
| Field basic | Frozen production engine, Grain/Pitch/Motion/Texture/Focus/loop mix off, material response enabled |
| Field default features | Same engine, default Grain/Pitch/Motion, Texture 0.70; Mass follows nominal-decay fixture |
| Bare8 | Eight-line Householder FDN without input diffusion or early taps: control experiment |
| House16 | 16-line Householder, stereo four-stage input allpasses, explicit early taps |
| Hadamard16 | Same delays, diffusion, taps, gains as House16; normalized Hadamard matrix replaces Householder |
| Schroeder | Eight independent damped comb returns following stereo allpass diffusion; fixed delay lengths |
| Cross-tank | Two cross-coupled paths, modulated allpass then long delay, damping, allpass and long delay; tapped stereo outputs |

The cross-tank uses original lengths and taps, structurally informed by published
allpass-tank work; it is not a Lexicon emulation or an exact Dattorro implementation.
The simple Schroeder control is also not a claimed commercial-quality reference.
All candidates are linear apart from time variation and bounded input sanitizing;
no distortion, pitch, granular transformation, capture or FOREVER features were
added. The research controls are decay, high-frequency decay ratio, diffusion
and small modulation. They are not a proposed public parameter contract.

FDN delay gain is `10^(-3 * delaySeconds / decaySeconds)`, independently for each
line. A low/high shelf approximation uses two gains and a 3.5 kHz one-pole split;
therefore High Ratio is an asymptotic target, not an exact octave RT60 promise.
The cross-tank uses estimated half-path lengths, including allpass lengths;
frequency-dependent allpass group delay limits exact calibration. Feedback is
attenuating, matrix transforms normalized, and allpasses bounded below unity.
Interpolation/modulation can still change energy and must be assessed by stress
and decay measurements, rather than assuming a lossless matrix proves stability.

Controls initialize at the first requested values, then slew over 30 ms.
Coefficients update every 32 samples with a persistent sample counter, not once
per host block. Allocations occur at prepare only. Fixed topology, finite delay
storage, deterministic phase modulation, no callback allocations/locks/files.
Reset is a lifecycle operation; its bulk clears are not offered as an audio-thread
performance gesture. There is no production/UI integration in this laboratory.

## Real commercial reference renders

The laptop's installed **FabFilter Pro-R 1.13** and **UADx Lexicon 224 1.4.16**
were actually instantiated and rendered through a small macOS Audio Unit host.
They were not merely cited as hypothetical references. Exact component versions
and complete exposed parameter/default values are in the logs. Pro-R is the
original Pro-R, not Pro-R 2. UAD program parameter was 5; no undocumented program
name or historical equipment inference is made.

Pro-R Mix parameter 6 was set to 100; UAD Mix 1568 was set to 1, with existing
Wet Solo=1. Defaults otherwise remain intact, including their own decay/tone
choices. Half a second of silent warm-up follows each reset before the identical
source begins. The unprimed UAD first impulse was suppressed: that pass is kept
as a rejected artifact and excluded from final comparisons. The warmed pass has
nonzero, measurable impulse output and normal musical-source renders.

Both were rendered at 48 kHz, stereo, 512 frames; there was no Cubase session or
GUI preset manipulation. This is a suitable-host comparison, not a claim of
Cubase validation. Licensing/plugin internals are not modified or copied. All
source audio is generated locally; speech uses macOS Samantha, not a protected
recording or a natural sung performance.

## Evidence and listening

Open `build-evidence/f01-foundation/reference-comparison/index.html` for the
**45-file, nine-way listening comparison**. It uses identical impulses,
percussion, chords, sustained tones and spoken vocals. Each source has one common
integrated-loudness target across all nine paths, peak headroom <=0.8, and match
error <=0.05 LU. Measurements retain input levels and applied gain. A single-pass
match failed on quiet impulses due to loudness gating; the final iterative match
is verified after applying gain. The nominal internal pack has 61 WAVs: five dry,
35 wet comparisons and 21 raw 20-second IRs across 0.6/2.4/8 s requests.

The final internal pack is `build-evidence/f01-foundation/final/`. Raw warmed
commercial output is in `pro-r-warmed/` and `uad-224-warmed/`. The reference IR
measurements use the same .25 impulse across all nine paths, independent of
post-render gain. Original raw internal IRs use .1. No copyrighted audio was
extracted from any commercial recording.

[Measured results and caveats](EVIDENCE.md) separate facts from hypotheses.
No generated audio was subjectively auditioned by the agent. Smoothness,
beautiful space, natural vocals, depth and perceived metallic ringing cannot be
accepted from graphs. In particular, Pro-R's early density proxy is lower than
several candidates: that does not mean the candidates sound better than Pro-R.

Listen first at ordinary level to percussion and speech, with effects off.
Compare transient continuity, flutter, consonant smearing, pitch stability,
low-mid buildup, stereo placement, mono collapse and tail texture. Then compare
chords and sustained tones for ringing or modulation that imposes pitch. Repeat
blind, swap order, and assess quiet and loud input. Keep raw decay differences
visible: level matching is not decay matching. A second controlled reference
preset/RT round and natural sung vocals remain useful follow-ups, not completed
experiments.

## Recommendation and compatibility plan

For the next musical evaluation, shortlist **Hadamard16** and **Cross-tank**, with
House16 as the direct matrix comparison. Hadamard16 is the provisional production
architecture proposal: independent line decay, input diffusion, an explicit
stereo early section and a separate late return. This is a recommendation for
listening and refinement, not approval to ship. Do not choose a winner by density
or correlation. Early taps, modulation/interpolation and band-decay mapping still
need listening and calibration. The fixed 3.5 kHz split is deliberately simple;
it is not a finished perceptual tonal design.

If a candidate passes listening, keep existing Field DSP and old state versions
as a selectable legacy engine. Add a distinct validated engine-revision property;
absence must select the old engine, since existing legacy/material flags already
have meanings. Keep existing parameter IDs/order and saved values. Make adoption
explicit with a useful starting translation, preserve original presets, and add
new presets only for the accepted engine. Parameter range/control mappings should
follow a musical evaluation, not be inferred from this experimental Controls
struct. Unknown revisions reject a state transaction, rather than guessing.
Regression tests must cover old-engine sample-exact fixture recall, migrations,
Cubase automation, transport, latency and reopening projects before deployment.

Only after conventional quality passes should stored-energy evolution return.
Separate decay/tone from pitch/grain transforms and meter energy before/after
those branches; do not compensate blindly with an automatic boost. FOREVER must
be evaluated for spectral persistence and controllable release without hiding a
poor reverb onset. No new experimental feature is authorized by this recommendation.

## Reproduce

Build this directory with CMake Release and run `ctest`. The project does not
change the root product build. `-DF01_SANITIZERS=ON` enables ASan/UBSan.

```
f01_lab render fresh-folder spoken-source.wav
/usr/bin/python3 research/f01-foundation/analyze.py fresh-folder
f01_lab benchmark fresh-benchmark-folder
```

The analysis uses system Python's NumPy, without installing packages. Speech was
made with `/usr/bin/say -v Samantha` and converted to float32 48 kHz WAV using
`afconvert`. `f01_au_reference type subtype manufacturer` prints parameters;
append source folder, fresh output, wet parameter ID and value to render.
Pro-R codes: `aumf FPRr FabF`; UADx: `aufx U39N UADx`. The Audio Unit registry
was inaccessible in the sandbox, so only this bounded reference-host operation
ran with approved system access. `f01_reference_match` takes internal/pro-r/uad
folders and a fresh destination. See logs and metadata for the exact final inputs.

## Primary research sources

- [Julius O. Smith, FDN reverberation](https://ccrma.stanford.edu/~jos/pasp/FDN_Reverberation.html): framework for matrix, delay-length and damping alternatives.
- [Normalized Hadamard construction](https://ccrma.stanford.edu/~jos/pasp/Hadamard_Matrix.html): orthogonal mixing used in the matrix comparison.
- [Jon Dattorro, Effect Design Part 1](https://nagasm.org/ASL/Sketch14/fig5/EffectDesignPart1.pdf): published input-diffusion/cross-tank alternative and interpolation considerations. Our implementation uses different lengths/taps.
- [UAD Lexicon 224 manual](https://help.uaudio.com/hc/en-us/articles/4419497193492-Lexicon-224-Digital-Reverb-Manual): reference controls for diffusion, tone-dependent decay and depth. No proprietary algorithm reconstruction.
- [Original Pro-R manual](https://www.fabfilter.com/help/ffpror-manual.pdf): reference for the installed version's spatial and decay controls.

The references informed competing hypotheses; measured results describe only the
implemented code and exercised settings. No claim that a set of known DSP
components guarantees excellent reverberation is made.
