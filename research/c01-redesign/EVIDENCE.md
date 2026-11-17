# Completed evidence — C-01 0.11.0

2026-11-17, local macOS arm64 build; universal VST3 also tested through Rosetta.

- Baseline: six original-engine WAVs rendered before revision.
- Final listening pack: 38 stereo 48 kHz float WAVs, MIDI CSVs, exact controls in
  `score.hpp`, level measurements, spectral CSV and SHA256 manifest.
- Isolated contracts: pass, including six sample rates (44.1–192 kHz), extreme
  controls, finite output, invalid inputs, sample-offset MIDI, panic, silence,
  allocation/OS-call audits and exact output across different block partitions.
- Separate ASan/UBSan build: 1/1 pass, 16.90 seconds.
- Product checks: 3/3 pass (engine, adapter/editor and VST3 host), 3.94 seconds.
- Intel host: 12 parameters, 16-sample latency, six block sizes, finite output,
  state recall. Native host passes the same product checks.
- Compatibility: old state with no circuit property selects Classic; Classic
  stereo output is sample-identical to the original engine for the exercised
  MIDI/parameter fixture. Adoption preserves every parameter. Dense state round
  trips; unknown revisions reject the whole state. Reprepare latency regression
  remains covered so the former host-restart loop does not recur.
- Product/prototype circuit source comparison matches after include-name change.

## What the measurements demonstrate

For the same central phrase, the mono spectral energy fraction at 100–800 Hz is
0.534 original, 0.894 dense source, 0.938 resonant, 0.844 dirty return and 0.851
full interaction. These are windowed spectral measurements, not perceived body
scores. At 10–12 seconds, original/dense/resonant short-stab paths are zero;
matched dirty-return/full paths have stereo RMS 0.00532 / 0.00502. This proves
retained audio, not aesthetic quality or a specified audible tail length.

A controlled causal test primes two otherwise identical networks, then enables
space-to-source interaction in only one. The next dry chord differs (about 0.27
relative RMS in the tested fixture). One-way Feedback dry output equals Dense
exactly; Full dry at Feedback=0 equals Resonant exactly. Thus the interaction is
not merely an additive tail or a hidden always-on modulation.

At MIDI note 105, the off-harmonic proxy improves from -34.40 dB at 1x to -58.56
at 2x and -72.46 at 4x, approximately the 8x result. At note 93, 2x/4x/8x remain
near -50.69 dB, so that probe is floor limited. Harmonic masks/window leakage and
legitimate nonlinear sidebands limit interpretation. This does not prove that all
settings are alias free. The existing rate-dependent 4x/2x/1x policy is retained.

## Performance

The first final benchmark ran alongside analysis and host activity; keep its
outliers visible in `benchmark-final`, rather than selectively deleting them.
The subsequent isolated run is in `benchmark-idle`. For Full, 12 held voices and
maximum main controls, 128-frame blocks:

| Rate | Median us | p99 us | Max us | Block deadline us |
|---|---:|---:|---:|---:|
| 48 kHz | 917.416 | 957.459 | 972.959 | 2666.667 |
| 96 kHz | 480.375 | 500.375 | 532.333 | 1333.333 |
| 192 kHz | 244.750 | 257.041 | 259.917 | 666.667 |

No safety-cap interventions in either measured run. Preallocated delay storage
is 460,864 / 921,664 / 1,843,264 bytes respectively, in addition to fixed object
state. All four candidates are recorded in the CSV, 200 measured blocks each.
This is a short local budget observation, not a worst-case CPU guarantee; the
contended run shows why OS scheduling and simultaneous tasks matter.

## Still unverified

No subjective audition of the generated audio or reference recordings was
performed by the agent. No claim of matching Basic Channel, convincing dirt,
physicality or exceptional musical character follows from these measurements.
Cubase musical acceptance and old real-project recall require the musician's
session. Suitable-host success is reported separately from Cubase. The complete
architecture is a concrete candidate for that evaluation, not an assertion that
all artistic acceptance criteria have passed.
