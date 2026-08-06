# Testing strategy

The first executable check covers the failure modes introduced by the first
signal path:

- finite output for silence, extreme/non-finite input, six sample rates, and
  the required odd and even block sizes;
- exact dry samples and zero latency when protection is disabled;
- deterministic output after reset;
- identical stereo input remains identical under linked detection;
- continuous stereo-link endpoints preserve the dominant channel while
  monotonically increasing reduction on the weaker channel;
- monotonic internal Density mappings;
- no heap allocation inside `process`.

CTest runs the check. `density_lab` renders a deterministic amplitude-stepped
sine to CSV and prints machine-readable peak/RMS/gain-reduction measurements.
Its `--benchmark` mode prints a machine-readable DSP-only CPU baseline under
block-rate Density and Stereo automation. `--detector-compare` renders
sample-level current-peak, RMS/peak, dual-time, programme-memory, hybrid
feed-forward, and feedback-inspired gain-reduction traces to CSV, explicitly
resets state between isolated events, checks finite release, sustained-level
calibration, transient differentiation, and bounded feedback-solve residual,
and runs in CTest. `--detector-auditions` generates four deterministic 48 kHz
Float32 WAV sets, RMS-matches each set within 0.001 dB, validates every write,
and also runs in CTest. `--detector-blind` turns those renders into twenty
deterministically randomized current-versus-candidate trials with anonymous
A/B audio, a blank response sheet, and a separate answer key.
Sanitizers are enabled with `-DASTE_SANITIZERS=ON` for non-real-time builds.

The VST3 adapter adds deterministic state round-trip, malformed-state,
mono/stereo layout, latency, finite audio, meter, exact-entry, reset,
accessibility exposure, resize-contract, and headless editor-paint checks. A locally built
pluginval v1.0.4 strictness-10 run covers editor lifecycle, background-thread
state, automation, parameter fuzzing, buses, all six sample rates, and all
thirteen requested block sizes. The independent Steinberg VST3 SDK 3.8.0
validator passes all 47 tests, including bypass, state transitions, variable
blocks, silence flags, automation, and threaded processing.

The plugin test executable's `--editor-artifacts` mode renders the active
980×540 panel at 2× scale and reports average full-panel software paint time
over 120 idle and active frames. CTest regenerates the artifact and fails on a
missing or unwritable PNG; visual acceptance remains an explicit review.

Golden audio, perceptual tolerances, UI image regression, and Cubase/Ableton
matrices remain. Golden updates will be explicit review actions, never
automatic.
