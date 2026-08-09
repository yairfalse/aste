# Specification status

This is the authoritative map from the master build brief to repository
evidence. It prevents an automated test, a design decision, and a real-world
validation result from being reported as the same thing.

Status meanings:

- **Done** — implemented and backed by repository evidence.
- **Accepted** — intentionally omitted or fixed by a recorded product decision.
- **External** — completion requires a named DAW, machine, listener, or musical
  project that is unavailable to repository automation.
- **Later** — required for a later product milestone, not Density D-01.
- **Open** — locally closable work. Internal beta is not specification-complete
  while any row has this status.

## Density product and DSP

| Requirement | Status | Evidence or closing action |
|---|---|---|
| Original mastering-oriented parallel dynamics instrument | Done | Product boundary and philosophy in [ARCHITECTURE.md](ARCHITECTURE.md) and [docs/PRODUCT_PHILOSOPHY.md](PRODUCT_PHILOSOPHY.md). |
| Independent clean and crush branches | Done | `DensityProcessor` graph and production golden renders. |
| Phase-aligned dry path | Done | Zero measured branch latency; differential dry-path tests. |
| Host latency matches algorithmic latency | Done | Production reports and measures zero at all supported rates. |
| Drive, Crush, Attack, Release, Density, Blend, Stereo, Output | Done | Stable host parameters and controls in [PARAMETER_MODEL.md](PARAMETER_MODEL.md). |
| Detector high-pass and output protection | Done | Stable parameters, detector response tests, documented sample-peak protection. |
| Density is a continuous monotonic macro | Done | Maintainer mapping and objective monotonicity tests; perceptual ranking remains external. |
| Very high ratio, hard knee, fast attack, nonlinear release | Done | Production gain computer and envelope measurements. |
| At least six detector/gain-control prototypes | Done | Current peak, RMS/peak, dual time, programme memory, hybrid feed-forward, and feedback-inspired lab comparisons. |
| Fully linked, partially linked, independent stereo | Done | Continuous Stereo endpoint and interpolation tests. |
| Stable image on adversarial stereo fixtures | Done | Objective correlation/polarity fixtures pass; perceptual A/B/C remains external. |
| Saturation and controlled clipping | Done | Production stages, alias reports, and golden renders. |
| Honest output-protection claim | Done | Explicitly sample-peak protection, not true-peak limiting. |
| Production quality/oversampling behavior | Accepted | [ADR 0002](adr/0002-density-quality-latency.md) fixes 1x/zero latency; measured 4x candidates remain lab-only because whole-instance CPU exceeds budget. |
| Auto gain | Accepted | Omitted until a mapping is proven musically trustworthy. |
| Amplifier stage | Accepted | Optional in Density; no unvalidated coloration stage was added. |

## Parameters, state, and interaction

| Requirement | Status | Evidence or closing action |
|---|---|---|
| Stable IDs, names, units, ranges, defaults, tapers, smoothing | Done | [PARAMETER_MODEL.md](PARAMETER_MODEL.md), adapter assertions, and automation reports. |
| Drag, fine adjustment, exact entry, reset | Done | JUCE host-standard interaction plus parser/reset tests. |
| Value-to-text and strict text-to-value conversion | Done | UI conversion and finite-decimal regression cases. |
| Automation smoothing without topology changes | Done | Nine individual and simultaneous torture renders; selected smoother differential tests. |
| Versioned portable state | Done | XML scalar schema in [STATE_FORMAT.md](STATE_FORMAT.md). |
| Explicit schema migration boundary | Done | Schema 1 identity migration; unknown versions leave state unchanged. |
| Deterministic state recall | Done | Repeated byte-stable serialization and equivalent processing. |
| Malformed-state safety | Done | 3,072 deterministic byte and structured mutations. |
| Compact product preset menu | Done | Five product-local starting points apply stable parameters from one accessible header menu; no browser, filesystem, or new state field. |
| MIDI mapping | Accepted | Density exposes ordinary automatable VST3 parameters; no product-specific MIDI behavior is required. |
| Host transport abstraction | Later | Density does not consume transport. Add only for Loop or Impulse, its first real consumers. |

## Real-time and numerical correctness

| Requirement | Status | Evidence or closing action |
|---|---|---|
| No audio-thread allocation or free | Done | Global allocation audit across 896 adapter lifecycle calls. |
| No audio-thread locks, waits, file access, or logging | Done | Calibrated macOS callback interposition audit. |
| No process-boundary exception | Done | Core processing is `noexcept`; finite adapter lifecycle tests. |
| Preallocated bounded state | Done | Fixed processor state and fixed lab-only delay storage. |
| Lock-free UI meter publication justified | Done | Compile-time lock-free `atomic<float>` requirement and bounded 30 Hz reader. |
| Denormals, NaN, infinity, silence, extremes | Done | Core and adapter adversarial input tests. |
| Sample-rate and block-size changes | Done | Repeated lifecycle matrix and variable-block production consistency. |
| Zero and non-power-of-two blocks | Done | Zero lifecycle calls and all specified odd/even sizes. |
| Finite output under fuzzed configuration | Done | Parameter, state, rate, layout, and lifecycle fuzz coverage. |

## UI system

| Requirement | Status | Evidence or closing action |
|---|---|---|
| Distinct non-skeuomorphic family language | Done | [UI_SYSTEM.md](UI_SYSTEM.md) and reviewed 1960×1080 artifact. |
| Density hierarchy and burgundy accent | Done | Hero Density, large GR display, nearby Drive/Blend, separated Output, boundary meters. |
| No tabs or hidden essential controls | Done | Single 980×540 surface. |
| Scaling and stable layout | Done | 760×420–1520×840 contract and headless 1x/2x checks. |
| Informational motion only | Done | Input, output, and gain-reduction meter motion. |
| Keyboard order and accessibility metadata | Done | Explicit ten-control focus order, titles, descriptions, and tests. |
| Native Retina, VoiceOver, and first-use behavior | External | Execute the host transfer in [HOST_COMPATIBILITY.md](HOST_COMPATIBILITY.md). |
| Visual regression review | Done | Deterministic editor artifact generation; acceptance remains explicit rather than auto-updated. |

## Research and historical evidence

| Requirement | Status | Evidence or closing action |
|---|---|---|
| Lawful schematic source catalog with required metadata | Done | Link-only [catalog.yaml](research/schematics/catalog.yaml). |
| 1970s/1980s compressor, limiter, line-amplifier, clipping references | Done | Source ledger and extracted engineering principles. |
| No redistributed manuals, firmware, IR libraries, samples, or branding | Done | Repository policy and source inventory. |
| Fidelity vocabulary tied to evidence | Done | No exact-emulation claim; measured production claims only. |
| Nonlinear alias evaluation | Done | Per-stage six-rate report and oversampling references/prototypes. |
| Shared amplifier platform | Later | Extract when Density adopts a justified stage or Harmonic becomes the second consumer; no speculative framework. |

## Build, validation, and delivery

| Requirement | Status | Evidence or closing action |
|---|---|---|
| Modern C++20, CMake, strict warnings | Done | All product targets use C++20 and `-Wall -Wextra -Wpedantic -Werror`. |
| Framework decision before integration | Done | [ADR 0001](adr/0001-plugin-framework.md). |
| Independent VST3 binary | Done | Universal Density D-01 VST3 bundle. |
| arm64 and x86_64 universal artifact | Done | CI slice and signature inspection. |
| Formatting gate | Done | Tracked C++ is normalized by `.clang-format`; CI runs check-only verification. |
| Static-analysis gate | Done | Apple Clang path-sensitive analysis covers production DSP with warnings as errors. |
| Unit, property, differential, regression, fuzz, real-time tests | Done | 33 CTest gates plus validator matrices; properties are expressed directly without a speculative framework. |
| Sanitizer build | Done | ASan/UBSan VST3 CI job. |
| Plugin validation | Done | pluginval strictness 10 locally and pinned Steinberg extensive validation in CI. |
| License/dependency audit | Done | SPDX package inventory, notices, exact pins, and expiring advisory review. |
| Deterministic package rehearsal | Done | Reopened ZIP with immutable file inventory, provenance, signature, architecture, SBOM, and security-evidence checks. |
| Safe user-scoped macOS installation | Done | Native script verifies the universal bundle, preserves any prior copy, installs without `sudo`, and re-verifies the result. |
| Immutable third-party CI action pins | Done | Every remote workflow action uses a full commit; repository policy rejects floating refs. |
| Final company identity and bundle identifier | External | Requires the selected legal/product identity; neutral `aste`/`density-d01` remain internal. |
| Developer ID signing and notarization | External | Requires Apple credentials and distribution approval. |

## Release and musical validation

| Requirement | Status | Evidence or closing action |
|---|---|---|
| Cubase 14, Ableton Live 13/beta, additional host | External | Execute the documented host matrix. |
| Offline, realtime, freeze, reopen equivalence | External | Capture target-host results and render comparisons. |
| Oldest supported Apple Silicon/native Intel budgets | External | Run the fixed benchmark on named hardware. |
| Automation, Density, stereo, detector, oversampling listening | External | Complete the generated blind response sheets before opening keys. |
| Ten real musical projects | External | Record project, material class, settings, failures, and recall result. |
| Operable without manual | External | Independent first-use observation in a target host. |
| All 25 Density release gates | External | The detailed pass/pending ledger is [RELEASE_CHECKLIST.md](RELEASE_CHECKLIST.md). |

## Family boundary

The repository architecture, visual language, historical-research policy, and
product independence rules apply to all four instruments. Harmonic's
[research specification](products/harmonic/SPECIFICATION.md) is frozen;
its lab algorithms remain **Later** than Density external validation. Loop and
Impulse are deliberately later implementation milestones. Every product's DSP,
parameters, state, UI, and regression fixtures remain product-owned. Density
does not gain speculative transport, pitch, rhythm, modulation, or amplifier
libraries to make the future directory tree look complete.

## Internal-beta completion rule

Density becomes **specification-complete internal beta** when every **Open** row
above is either **Done** with evidence or **Accepted** by an ADR. This label does
not mean release-ready: every **External** row remains a hard release gate.

As of 2026-08-09 there are zero **Open** rows. Density is therefore
specification-complete for internal beta, subject to a green build and test run
for the declaring commit. It is not release-ready and makes no Cubase, Ableton,
native-Intel performance, listening, notarization, or real-project claim.
