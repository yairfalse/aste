# C-01 retained evidence — 2026-11-16

Reproduction commands and local toolchain setup are in
[the laboratory README](../../../research/c01/README.md). Musical interpretation,
method limits and listening procedure are in [RESEARCH.md](RESEARCH.md).

## Artifacts

- `build-c01-listening-final`: 38 studies, 76 WAVs (raw plus matched), sample event
  traces, second-spaced actual state observations, measurements, alias study,
  response sheet, key and source/artifact SHA-256 provenance.
- `build-c01-family-final`: four RMS-matched family references and conditions.
- `build-c01-reproduction`: independent rerender for byte comparison.
- `build-c01-plugin/ContaminationC01_artefacts/Release/VST3/Contamination C-01.vst3`:
  native arm64 development bundle; no install, no universal/release claim.
- `build-c01-plugin/c01-editor.png`: native minimum-size component snapshot.

Build directories and WAVs follow repository ignore policy. Small measurement
CSVs, state trace and provenance are retained in [evidence](evidence).
Approximate pack render timings may include competing work; use the dedicated
serial benchmark for budgeting. The source manifest includes current comparator
processors because HEAD alone does not capture this dirty workspace.

## Findings separated from judgments

| Check / measurement | Result | What it does not prove |
|---|---|---|
| Identical-chord reset | First/last normalized difference 0 | Musical quality |
| Local / shared / hybrid memory | Differences 1.037 / 1.262 / .855 | Perceived relationship or preference |
| Instability 0 / .4 / 1 | Differences 0 / .064 / .152 | Chaos, nonlinear bifurcations |
| Pressure 0 / .5 / 1 centroid | 196 / 248 / 381 Hz | Physical weight or desirable darkness |
| Pressure raw RMS | .03748 / .03026 / .02521 | Equal perceived loudness |
| Noise output vs return, white | >8 kHz power fraction .000409 vs .00000382 | Structural-noise preference |
| Whole listening pack | Zero nonfinite observations | All possible automation/MIDI sequences |
| MIDI93, max pressure off-harmonic proxy | −30.4 / −51.8 / −70.0 / −70.0 dB at 1/2/4/8× | Exact alias power |
| MIDI105, max pressure proxy | −1.0 / −29.2 / −54.5 / −66.1 dB | Alias-free high-register extremes |

First/last values compare corresponding windows of repetitions 1 and 16.
They are waveform differences, not percentages or preference scores. Memory
conditions use the same fixed seed and zero Instability. A separate counterfactual
contract test compares the same future note after prior excitation versus an
empty history, then checks that disabled Memory and CLEAR remove the effect.
This controls for surviving release/DC state rather than merely checking unequal
waveforms after unrelated parameter changes.

## CPU and memory

AppleClang 17.0.0, native arm64, Release/O3; machine scheduling is uncontrolled.
Dedicated benchmark: twelve held voices, all principal nonlinear controls at
maximum, 128-frame blocks, 50 warmup blocks, 500 measured blocks per condition.
`fraction` means measured callback wall time / corresponding audio duration.
The benchmark ran without a competing build or render. This is one short serial
run, not a worst-case hardware/DAW deadline guarantee.

| Host rate | Product factor | Mean fraction | P99 fraction | Max fraction |
|---|---:|---:|---:|---:|
| 22.05 kHz | 8× | .264 | .290 | .586 |
| 44.1 kHz | 4× | .262 | .287 | .291 |
| 48 kHz | 4× | .285 | .319 | .361 |
| 96 kHz | 2× | .287 | .317 | .346 |
| 192 kHz | 1× | .295 | .355 | .374 |

Research engine storage is **8200 bytes**, fixed, with no DSP heap allocation.
This excludes JUCE/editor/host memory. Candidate budget: mean below .35 and P99
below .45 of one core in this specific twelve-voice test, engine below 16 KiB.
The measured sample-rate policy meets it in this run. Fixed 4× at 192 kHz used
1.122 mean fraction and was rejected for the product. Fixed 8× at 96 kHz also
missed real time. This evidence prompted the preparation-time factor policy;
there is no audio-thread topology switch. Further optimization and sustained
multi-instance/host load testing remain required.

## Verification scope

All 76 main-pack WAVs match byte for byte on an independent rerender with the
same compiler/platform. The final Release contract suite, ASan/UBSan suite and
JUCE plugin contracts pass.

Release C++ contract suite and AddressSanitizer/UndefinedBehaviorSanitizer suite
cover repeatable restart, block partition invariance (1 versus 127 frames),
zero-memory repeat consistency, causal history/Clear counterfactuals, all supported
factor choices at five sample rates, extreme controls, invalid rate/factor,
nonfinite controls/events, bounded finite output, pedal hold, stealing and panic.

Both engine and JUCE callbacks are checked for new/new[] activity; JUCE also
counts ordinary delete calls. The macOS interposition audit observes zero
mutex/file-open/write calls in the sampled callback paths. This is runtime
evidence for those paths, together with fixed-storage source inspection, not
an exhaustive proof about every host or aligned allocator API.

Plugin tests cover the 12-parameter count, 16-sample latency, scalar recall,
rejection of malformed restore, exact MIDI sample dispatch, panic silence,
callback audits, editor construction and minimum-size snapshot. The repository's
actual VST3 smoke host reports one named component, 12 parameters, 16 samples
latency, successful state round trip and finite output over six blocks. Ad-hoc
signature verification passes. These checks do not establish Cubase compatibility,
VoiceOver usability, HiDPI quality or musical acceptance.

Human listening scores are **uncollected**. No native Cubase session, VST3 SDK
validator, universal/x86_64 build or long-session host/denormal stress has been
claimed. The outstanding work is explicit in [PLAN.md](PLAN.md).
