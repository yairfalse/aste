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

`harmonic_lab` began as the separate pre-product research boundary for Harmonic
H-01 and now also exercises the selected product graph.
`--compare` covers the double-precision peaking reference and rejected
residual-excitation candidate. `--preemphasis` renders five centers, four input
levels, clean cuts, a canonical macro sweep, a folded odd-harmonic proxy, and
third-order SMPTE/CCIF-style fixtures at all six sample rates. Both commands run
in CTest and write CSV. The second command reports measurement validity and the
separate frozen-gate decision, so a rejected candidate remains reproducible
evidence rather than a broken test. Harmonic collisions with the fundamental
are marked unobservable instead of being misreported as distortion.
`--stateful` applies that same matrix to the bounded state-variable candidate
and adds neutral-silence, impulse, step, overload, two-second recovery, and
steady-state error evidence. It also runs in CTest; the frozen advance decision
is reported separately from finite measurement validity.

`--product-report` runs the complete four-band serial processor at all six
supported rates with broad-boost and boost/cut fixtures, recording RMS gain,
peak, harmonic activity, latency, and finite status. `--product-benchmark`
changes every band and the Harmonic macro at block rate and reports a five-run
median without placing timing in sanitizer gates. `harmonic_tests` independently
checks finite output, exact neutral behavior, zero latency, stereo identity,
deterministic reset, variable-block identity, clean cuts, monotonic canonical
H3, and zero processing allocation at the required rates and block sizes.

`sequence_lab` renders two seconds of the host-clocked production voice to CSV
and reports peak, RMS, and finite status. Its `--benchmark` mode exercises the
full monophonic voice and nonlinear character filter. `sequence_tests`
covers MIDI synthesis, six rates, irregular and variable blocks, deterministic
reset, absolute-PPQ step selection, hostile parameters, zero latency, monotonic
Pressure mapping, shared-state filter continuity without midpoint collapse,
and zero process allocation. The adapter suite locks all 85
voice/pattern parameters, state recall/fuzz behavior, MIDI instrument buses,
bypass silence, editor visibility/scaling, and the VST3 ABI.

`impulse_tests` covers deterministic block partitioning, six sample rates,
eight MIDI objects, exact manual-pattern scheduling, finite output, zero
latency, and zero callback allocation. Adapter tests lock all 368 parameters,
schema-1 migration, state fuzzing, real-time forbidden-operation traps, the
always-visible 8×32 grid, selected sound controls, and the VST3 ABI.

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
CTest also checks the required-document set, local Markdown links, and configured
build provenance. CI records the same results as JUnit XML.

CI checks every tracked C++ source against the repository `.clang-format` file.
Apple Clang's path-sensitive static analyzer runs separately over all
framework-independent production DSP targets with warnings treated as errors.
The JUCE adapter retains strict compiler warnings and the independent runtime,
fuzz, sanitizer, pluginval, and Steinberg validator boundaries described below.

Each VST3 adapter adds deterministic state round-trip, malformed-state,
mono/stereo layout, latency, finite audio, meter, exact-entry, reset,
accessibility exposure, explicit ten-control keyboard focus order,
resize-contract, and headless editor-paint checks. A locally built
pluginval v1.0.4 strictness-10 run covers editor lifecycle, background-thread
state, automation, parameter fuzzing, buses, all six sample rates, and all
thirteen requested block sizes. The independent Steinberg VST3 SDK 3.8.0
validator's extensive suite passes 537 tests, including bypass, state
transitions, variable blocks, silence flags, automation, and threaded processing.
The universal VST3 CI job rebuilds that validator from pinned MIT-licensed
source and requires the extensive suite to pass the signed bundle. The
validator is test tooling only and is not a product dependency. Harmonic's
adapter tests additionally lock its 12 parameter IDs/text contract, six factory
starting points, bypass, four-band UI exposure, and arbitrary-byte state fuzz.

After independent validation, CI builds the deterministic internal Density ZIP
and reopens it with `tools/package_density.py verify --require-clean`. The check
requires clean immutable provenance, the expected placeholder identity and
version, actual arm64/x86_64 slices and strict ad-hoc signature verification
after extraction, fixed ZIP metadata, safe paths, required bundle files, and a
complete SHA-256 inventory. CI does not upload this development archive.
The inspector also reconstructs the embedded SPDX 2.3 SBOM and requires exact
Density, pinned JUCE, and JUCE-bundled VST3 SDK package identities plus their
four reviewed relationships. Unresolved licence conclusions must remain
`NOASSERTION`.
The same inspector requires the exact embedded dependency-security ledger,
reconstructs its `PACKAGE.json` digest/date/expiry/disposition fields, and
re-runs its policy validation. Direct tampering is covered by the file
inventory; even a rewritten inventory is rejected by the repository-known
ledger digest. Default inspection uses the current date. `--as-of` is reserved
for expiry tests and historical inspection, not release approval.

CTest also validates the reviewed dependency-security ledger without network
access. It requires OSV exact-commit, publisher-advisory, and NVD evidence for
both packaged dependencies, preserves the two historical JUCE CVE
dispositions, cross-checks the source and package pins, and fails after the
90-day review window. A dependency update therefore cannot reuse the previous
security conclusion.

`vst3_smoke_host` is the smallest repo-owned standalone host. Unlike the linked
adapter tests, it discovers and loads each built bundle through VST3, then
checks component identity, parameter count, latency, deterministic state, and
finite stereo processing across six irregular block sizes. Optional
`--write-state` and `--read-state` modes exchange one known non-default state
for arm64/x86_64 portability checks without adding a preset format.

The plugin test executable's `--editor-artifacts` mode renders the active
980×540 panel at 2× scale and reports average full-panel software paint time
over 120 idle and active frames. CTest regenerates the artifact and fails on a
missing or unwritable PNG; visual acceptance remains an explicit review.

The plugin test also counts global heap allocations around 896 full adapter
processing calls. It repeats prepare/process/release across mono and stereo,
bypass and active processing, abrupt parameter endpoints, six sample rates,
thirteen nonzero block sizes, and zero-length blocks.

On macOS, a linked test-only audit library uses dyld interposition to count
common POSIX and unfair lock acquisition, file opens, and direct writes on the
audited callback thread. A deliberate mutex/open/write calibration must be
observed before the zero-call assertion is accepted.

State restoration fuzzing uses fixed seed `0xd01f022` for 3,072 cases. It
combines arbitrary host bytes and mutations of valid state with structured XML
mutations that reach parameter validation. Every case checks finite in-range
parameters, byte-stable serialization, and identical finite audio after reset.
An invariant failure prints its case number and complete input bytes as hex.

`--golden` renders four production-DSP stereo WAVs and a deterministic CSV
manifest. CTest compares RMS, peak, crest factor, gain change, correlation, gain
reduction, and latency against the tracked baseline with documented tolerances.
The sample fingerprint is supplemental rather than the sole oracle. The command
never updates the baseline; accepted changes require an explicit CSV edit.

`--production-consistency` renders the same two-second continuous-time fixture
at 44.1, 48, 88.2, 96, 176.4, and 192 kHz. A fixed 127-sample render must be
sample-identical to a repeating schedule of all thirteen required nonzero block
sizes. Across rates, RMS gain and crest ranges must remain within 0.03 dB, peak
within 0.01 dB, maximum gain reduction within 0.10 dB, and stereo correlation
within 0.001. Latency must remain zero. Zero-length calls are covered by the
core and full-adapter lifecycle tests because they cannot advance a render.

`--alias-report` drives the production saturation, crush-path clip, and output
protection transfer functions with a coherent tone near 7 kHz. It reports the
folded energy attributable to odd harmonics 3 through 63 relative to the
fundamental at every supported rate. CTest verifies finite results and the
expected reduction at 192 kHz; it does not impose a release-quality alias
ceiling before oversampling candidates have been compared.

`--oversampling-report` compares the same nonlinear stages at 1x, 2x, and 4x
using a lab-only Blackman-windowed sinc reference with 64 taps per phase. It
records residual folded energy, fundamental gain shift, actual filter length,
and the equivalent causal round-trip latency. Circular convolution removes
startup transients from the coherent measurement; this is not the production
real-time implementation.

`--oversampling-prototype` differentially checks the streaming Density-local 4x
polyphase prototype at 16, 32, 48, and 64 taps per phase. Four coherent tones
cover the established 7 kHz case and the more demanding decimator transition
region. It measures residual alias, fundamental shift, impulse-peak latency,
variable-block identity, object size, and five-run stereo CPU medians. The
prototype is not connected to the product processor, so current sessions retain
zero latency and the existing golden. Sanitizer CTest runs retain correctness
checks but skip unoptimized instrumented timing.

`--halfband-prototype` applies the same four-tone, latency, variable-block, and
CPU protocol to a streaming two-stage 2x+2x half-band prototype. Six stage-
length configurations isolate the critical base-rate decimator from the upper
stage. Half-band zero coefficients are omitted from processing rather than
multiplied by zero. Sanitizers again skip only the timing loop.

`--kaiser-prototype` holds the sparse 113/33 topology fixed and replaces only
the first-stage prepare-time window. Blackman and Kaiser beta 3, 5, 7, 9, and
11 use the same four-tone residual-alias and gain protocol. CPU is not retimed
because coefficient values do not change the processing operations.

`--kaiser-sweep` compares Blackman, Kaiser beta 3, and Kaiser beta 5 at 0.25,
0.60, and 0.90 peak level across coherent odd-bin tones from 1 to 20 kHz. Odd
bins prevent folded harmonics from colliding with lower natural harmonics. The
Release matrix uses 500 Hz target spacing; sanitizer CTest uses a six-frequency
smoke matrix while retaining every level and window.

`--kaiser-rate-sweep` applies the same physical-frequency and level protocol to
Kaiser beta 3 and beta 5 at all six supported sample rates. The full matrix has
702 points per window; sanitizers use 108. If no measured odd harmonic through
63 crosses Nyquist, the folded-harmonic metric records its finite -300 dBc
floor instead of emitting infinity or NaN. Candidate gain-limit failures are
reported separately from measurement validity.

`--kaiser-linear-report` bypasses the nonlinear transfer inside the same
streaming 4x half-band prototype and measures coherent-bin magnitude plus
measured phase at 250 Hz target spacing. Phase is unwrapped to the nearest
64-sample linear-phase prediction and the residual is reported separately.
The full matrix contains 462 points per window; sanitizers use 36.

`--kaiser-length-report` compares β5 first-stage lengths 65, 73, 81, 89, 97,
and 113 with the second stage fixed at 33 taps. One row per topology combines
the 48 kHz three-level alias sweep, 44.1 kHz linear magnitude/phase sweep,
latency, and the existing five-run stereo CPU median. Sanitizers retain reduced
frequency matrices and skip only timing.

`--kaiser-finalist-report` runs the 73/33 and 81/33 β5 finalists through both
full six-rate matrices. It emits one aggregate row per topology and sample rate
covering three-level alias p95/worst case, weak-point count, linear magnitude,
phase residual, and latency. Sanitizers use the established reduced frequency
set.

`--oversampled-chain-report` activates the otherwise unreachable 73/33 β5
processor prototype. It checks dry and wet impulse latency, variable-block
identity, and five-run medians for both default and oversampled full signal
graphs. Sanitizers retain every correctness check and skip only timing; an
unmeasured timing run is never reported as passing the CPU budget.

`--oversampling-auditions` renders anonymous A/B pairs for the transient,
bass, dense, and ambient synthetic fixtures. It delays the 1x render by the
prototype's measured 44 samples, sample-RMS matches each pair within 0.001 dB,
and applies common peak normalization to -1 dBFS. A fixed seed makes assignment
reproducible; the answer key, response sheet, and measurements are separate.

`--automation-report` alternates each of the nine continuous parameters between
its endpoints every 127 samples, then repeats with all nine changing together.
At each boundary it compares output second difference with the largest nearby
second difference; this discounts ordinary carrier slope while retaining a
step or sudden trajectory change. CTest requires a complete finite report. The
CSV separately records a provisional -60 dBFS curvature-excess quality gate so
a valid measurement cannot be mistaken for a passing sonic result.

`--output-smoothing-report` isolates the Output parameter by rendering the
production graph at unity output gain, then applying four lab-only dB smoothers
before the real protection transfer. It reports boundary curvature, first-
sample motion, response after 5 ms, time to settle within 1 dB, and difference
from the actual production render. The selected 3+3 ms model must match
production sample-for-sample before comparison measurements are accepted.

`--drive-smoothing-report` runs the same four profile shapes through the full
production graph because Drive feeds both crushed audio and detector behavior.
A lab-only processor prepare path changes only Drive smoother coefficients and
stage count. The selected 3+3 ms profile must reproduce production sample-for-
sample; reports include the same curvature and response-lag metrics as Output.

`--attack-smoothing-report` compares the production 5 ms logarithmic Attack
trajectory with the former block-constant reference and two alternative
smoothers through the full detector graph. The report records curvature, 5 ms
response, settling to within 1% of the log range, and sample delta from current
production; the selected row must match production sample-for-sample.

`--blend-smoothing-report` compares four Blend trajectories through the full
dry/crush parallel graph. It records boundary curvature, response after 5 ms,
settling within 1%, and sample delta from production; the selected 3+3 ms row
must match production sample-for-sample.

`--automation-auditions` renders four anonymous, deterministic A/B pairs for
Drive, Attack, Blend, and simultaneous endpoint automation. Production and
the retained pre-smoothing reference are RMS matched, share -1 dBFS peak
normalization, and have separate answer and response sheets. CTest requires
all production renders to retain the -60 dBFS curvature ceiling.

`--stereo-stability-report` renders centered kick, hard-panned percussion,
correlated program, decorrelated ambience, mono-in-stereo, and polarity-
inverted fixtures at 0%, 50%, and 100% detector link. It records balance,
correlation, mid/side width, gain reduction, and exact identical/inverted
channel invariants without imposing an unvalidated perceptual threshold.

`--stereo-stability-auditions` reuses those fixtures as six anonymous A/B/C
trials containing independent, partial, and fully linked processing. All three
files are RMS matched, share -1 dBFS peak normalization, and use separate
answer and response sheets; centered, mono, and polarity-inverted trials must
remain exact null controls.

`--density-macro-auditions` renders anonymous A/B/C/D rankings for four source
types at 0%, 33%, 67%, and 100% Density. Files are RMS matched, share -1 dBFS
peak normalization, and include an exact dry-path null control; internal gain
reduction must remain monotonic before a pack is accepted.

Additional musical fixtures, UI image regression, and Cubase/Ableton matrices
remain.

## Field F-01 evidence

`field_tests` covers exact zero-blend dry output, zero latency, six rates,
non-finite controls, finite FOREVER feedback, MIDI excitation, deterministic
grain motion, variable block partitions, fixed-state memory budget, and zero
processing allocation. `field_plugin_tests` adds stable parameter count,
deterministic and malformed state, sample-offset MIDI, macOS callback-system
audit, meter publication, essential-control visibility, scaling, and a rendered
UI artifact. The independent VST3 smoke host verifies dynamic bundle loading,
state, irregular processing blocks, finite output, parameter identity, and
latency.

`field_lab OUTPUT.csv` renders impulse responses in release and FOREVER modes at
44.1, 48, 88.2, 96, 176.4, and 192 kHz. It records peak, first-second RMS,
last-second RMS, stored energy, finite status, and latency. `--benchmark` runs
all costly controls at maximum for ten seconds at 48 kHz / 127 samples. These
tests establish implementation behavior, not a musical reverb-quality claim;
the product music-machine protocol remains mandatory.
