# Aste Signal Instruments

[![CI](https://github.com/yairfalse/aste/actions/workflows/ci.yml/badge.svg)](https://github.com/yairfalse/aste/actions/workflows/ci.yml)

`Aste` is the neutral internal codename for a planned family of professional
audio instruments. It is not the company name and must not become a permanent
public namespace, bundle identifier, preset signature, or installer path.

The project combines original DSP, restrained industrial interfaces, historical
engineering research, deterministic measurement, and unusually strict
real-time testing. It is not a hardware-cloning exercise and does not use
“analog” as a substitute for describing measurable behaviour.

> **Project status:** internal research prototype. Density D-01 is the only
> implemented product. There is no supported release, installer, Developer ID
> signature, notarization, final company identity, or DAW compatibility claim.

## The instrument family

| Product | Purpose | Status |
|---|---|---|
| **Density D-01** | Parallel hard mastering compressor | Working VST3 prototype; active validation |
| **Harmonic H-01** | Equalizer with nonlinear band behaviour | Product definition only |
| **Loop L-01** | Tape-inspired playable memory instrument | Product definition only |
| **Impulse I-01** | Generative rhythm and transient instrument | Product definition only |

Density must become stable before implementation begins on the other three
products. Shared code is extracted only when two products genuinely need it or
when correctness requires a single implementation.

## Density D-01

Density is a mastering-oriented parallel dynamics instrument built around one
musical idea: increase weight, continuity, proximity, and controlled loudness
without erasing the dry signal's transient life and dimensionality.

It is not a generic compressor with a wet/dry knob. The architecture is
parallel from the start:

```text
input --+------------------------------ dry -------------------+
        |                                                       |
        + drive -> detector HPF -> continuous stereo link       |
                -> per-channel peak envelopes                   |
                -> hard gain computers -> saturation -> clip ---+-> blend
                                                                 -> output
                                                                 -> protection
```

The production graph currently has zero algorithmic latency. The dry and crush
branches are therefore sample-aligned without a delay line, and the VST3 reports
zero samples to the host. A measured 4× oversampling design remains isolated in
the DSP lab because the complete graph misses the default CPU budget and has not
passed musical selection. There is no hidden quality mode or silent topology
change.

### Controls

| Control | Range | Default | Role |
|---|---:|---:|---|
| Drive | -12 to +24 dB | 0 dB | Pushes the crush audio and detector |
| Crush | 0–100% | 65% | Sets the hard-compression contribution |
| Attack | 0.02–30 ms | 1 ms | Shapes transient entry |
| Release | 20–1200 ms | 180 ms | Sets recovery time |
| Density | 0–100% | 50% | Coordinates threshold, ratio, saturation, release curvature, and crush make-up |
| Blend | 0–100% | 50% | Mixes the aligned dry and crush paths |
| Stereo | 0–100% | 100% | Moves continuously from independent to fully linked detection |
| Output | -24 to +12 dB | 0 dB | Final level trim |
| Detector HPF | 20–300 Hz | 90 Hz | Reduces low-frequency detector dominance |
| Protection | Off/on | On | Sample-peak output protection |

Host bypass is the eleventh stable parameter. Every continuous control supports
host automation, exact text entry, reset, normalization, documented smoothing,
and deterministic state recall. Parameter IDs are treated as release contracts;
see [PARAMETER_MODEL.md](PARAMETER_MODEL.md).

### Sound and interface

At restrained settings Density is intended to increase presence and continuity
while keeping the transient-bearing dry path intact. At stronger settings the
crush branch becomes deliberately hard, close, and physical. The output stage
is protective sample-peak clipping, **not** a true-peak mastering limiter.

The current 980×540 editor scales from 760×420 to 1520×840. It uses a near-black
surface, off-white technical typography, a muted burgundy accent, explicit
keyboard order, editable values, and signal-driven meters. There are no tabs,
fake materials, rack decorations, or hidden essential controls. The full family
language is documented in [UI_SYSTEM.md](UI_SYSTEM.md).

## Requirements

The tested development environment is macOS on Apple Silicon, with x86_64
builds executed under Rosetta. You need:

- CMake 3.25 or newer;
- a C++20 compiler;
- Git;
- Python 3 for documentation and build-metadata checks;
- internet access when configuring the optional VST3 build, which fetches the
  pinned JUCE source.

The framework-independent DSP and lab build without JUCE or network access.
The VST3 build uses JUCE 8.0.13 at the product boundary; the DSP core contains
no JUCE headers.

## Build and test

### DSP core and laboratory

This is the fastest build and the right starting point for algorithm work:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

It builds:

- `density_dsp` — framework-independent production DSP;
- `density_tests` — core correctness and property checks;
- `density_lab` — offline rendering, measurement, comparison, and listening-pack
  generator.

### VST3, adapter tests, and standalone host

The VST3 build is opt-in because it fetches licensed JUCE source:

```sh
cmake -S . -B build-plugin \
  -DASTE_BUILD_VST3=ON \
  -DCMAKE_BUILD_TYPE=Release
cmake --build build-plugin --parallel
ctest --test-dir build-plugin --output-on-failure
```

The plugin bundle is produced at:

```text
build-plugin/DensityD01_artefacts/Release/VST3/Density D-01.vst3
```

`density_plugin_tests` constructs the JUCE adapter directly. The separate
`density_vst3_host` executable links neither the product adapter nor its DSP;
it discovers and loads the built bundle through the actual VST3 ABI, restores
state, and processes irregular block sizes.

To run that smoke host directly:

```sh
"build-plugin/density_vst3_host_artefacts/Release/density_vst3_host" \
  "build-plugin/DensityD01_artefacts/Release/VST3/Density D-01.vst3"
```

### Sanitizers

AddressSanitizer and UndefinedBehaviorSanitizer are intended for tests, not a
distributable plugin:

```sh
cmake -S . -B build-sanitize \
  -DASTE_BUILD_VST3=ON \
  -DASTE_SANITIZERS=ON \
  -DCMAKE_BUILD_TYPE=Debug
cmake --build build-sanitize --parallel
ctest --test-dir build-sanitize --output-on-failure
```

### Universal macOS bundle

```sh
cmake -S . -B build-universal \
  -DASTE_BUILD_VST3=ON \
  -DCMAKE_BUILD_TYPE=Release \
  '-DCMAKE_OSX_ARCHITECTURES=arm64;x86_64'
cmake --build build-universal --parallel
ctest --test-dir build-universal --output-on-failure
```

The internal bundle receives an ad-hoc signature so its completed manifest and
binary can be validated locally. It is not Developer ID signed or notarized.

## DSP laboratory

`density_lab` generates its own deterministic signals, renders WAV/CSV output,
benchmarks production code, compares detector and oversampling candidates, and
creates blinded listening packs without opening a DAW.

Common entry points:

| Area | Commands |
|---|---|
| Basic render and CPU | `./build/density_lab [output.csv]`, `--benchmark` |
| Production regression | `--golden`, `--production-consistency` |
| Detector research | `--detector-compare`, `--detector-auditions`, `--detector-blind` |
| Nonlinear behaviour | `--alias-report` |
| Oversampling research | `--oversampling-report`, `--oversampling-prototype`, `--halfband-prototype`, `--kaiser-prototype`, `--kaiser-sweep`, `--kaiser-rate-sweep`, `--kaiser-linear-report`, `--kaiser-length-report`, `--kaiser-finalist-report`, `--oversampled-chain-report` |
| Automation and smoothing | `--automation-report`, `--output-smoothing-report`, `--drive-smoothing-report`, `--attack-smoothing-report`, `--blend-smoothing-report` |
| Musical evaluation | `--oversampling-auditions`, `--automation-auditions`, `--stereo-stability-auditions`, `--density-macro-auditions` |
| Stereo analysis | `--stereo-stability-report` |

Most report modes accept an optional output file or directory after the flag.
CTest invokes every deterministic report with known paths. Measurement methods,
tolerances, and the meaning of each output column live in
[TESTING.md](TESTING.md); algorithm decisions and rejected candidates live in
[docs/products/density/DSP_RESEARCH.md](docs/products/density/DSP_RESEARCH.md).

## Engineering guarantees and evidence

The repository treats testing as part of the DSP architecture rather than a
release-week activity.

Current automated coverage includes:

- finite output under silence, non-finite/extreme input, abrupt automation, and
  malformed state;
- 44.1, 48, 88.2, 96, 176.4, and 192 kHz;
- zero-length blocks plus block sizes 1, 2, 7, 16, 32, 64, 127, 128, 256, 511,
  512, 1024, and 2048;
- mono and stereo layouts, linked/independent detection, bypass, repeated
  prepare/process/release, and variable block partitioning;
- exact dry-path preservation and measured/reported zero latency;
- 3,072 deterministic malformed-state cases;
- deterministic golden renders with metric tolerances rather than brittle audio
  hashes alone;
- global allocation counting across the core and JUCE adapter;
- calibrated macOS interposition checks for callback locks, file access, and
  direct writes;
- arm64 Release, arm64 ASan/UBSan, and x86_64 Release test trees;
- semantic state exchange between arm64 and x86_64 hosts.

The current plugin test tree contains 32 CTest checks. Independent local
validation has passed pluginval 1.0.4 at strictness 10 and the Steinberg VST3 SDK
3.8.0 extensive suite at 537/537 for arm64, x86_64, and the universal bundle.
The exact run history is in [HOST_COMPATIBILITY.md](HOST_COMPATIBILITY.md).

GitHub Actions builds the core on arm64 and Intel runners, builds a universal
VST3, verifies its architectures and ad-hoc signature, and runs an arm64
sanitizer tree. It also builds the pinned official Steinberg validator from
source and requires its extensive suite to pass the signed bundle. The workflow
is [`.github/workflows/ci.yml`](.github/workflows/ci.yml).

### Real-time rules

`DensityProcessor::process` is `noexcept`. Processing may not allocate, free,
lock, wait, log, access files or networks, resize containers, trigger lazy
initialization, call blocking OS services, or throw through the callback.
Buffers and mutable DSP state are prepared in advance. Non-finite input is
sanitized and output must remain finite. Thread ownership and the limits of the
runtime audit are documented in [REALTIME_SAFETY.md](REALTIME_SAFETY.md).

### State

Density uses a portable, versioned, product-local XML state document containing
stable parameter IDs and scalar values. Unknown parameters are ignored, missing
parameters receive defaults, duplicate IDs are rejected, and malformed state
leaves the current configuration unchanged. Schema 1 contains no speculative
quality or oversampling field. See [STATE_FORMAT.md](STATE_FORMAT.md).

### Performance

The current production graph measured a 0.266210% median of one M4 Pro
performance core at 48 kHz / 128 samples under worst-case block-rate automation.
This is comfortably below the provisional 1% target on that machine, but it is
not evidence for the oldest supported Apple Silicon or a native Intel Mac. Full
measurements and rejected oversampling budgets are in
[PERFORMANCE_BUDGETS.md](PERFORMANCE_BUDGETS.md).

## Current release status

Density has repository evidence for 16 of its 25 release gates. It remains an
internal prototype because the following work requires people, DAWs, or target
hardware rather than another synthetic unit test:

- Cubase 14, Ableton Live 13/beta, and one additional real-host matrix;
- real-time versus offline bounce, freeze, reopen, suspend, and crash-recovery
  checks;
- native Intel and oldest-supported Apple Silicon performance;
- Retina host rendering, VoiceOver, keyboard navigation, and independent
  first-use observation;
- completed blind automation, Density, stereo-link, detector, and oversampling
  listening sheets;
- ten real musical projects across mastering, ambient, percussion, bass-heavy,
  and sparse material;
- final signing, notarization, packaging, identity, and licensing review.

The authoritative ledger is [RELEASE_CHECKLIST.md](RELEASE_CHECKLIST.md), and
the next permitted work is in [ROADMAP.md](ROADMAP.md). Do not turn a green build
into a compatibility or fidelity claim.

## Architecture

```text
density_dsp  <- density_tests
      ^  ^
      |  +--- density_lab
      |
      +------ JUCE VST3 adapter/UI <- density_plugin_tests
                       ^
                       +---------- density_vst3_host (dynamic VST3 load)
```

The production DSP is ordinary C++20. JUCE is restricted to the VST3, host,
state-adapter, and UI boundary. Product DSP does not depend on widgets, and
serialized state does not depend on widget layouts. The accepted framework
decision is [ADR 0001](docs/adr/0001-plugin-framework.md); the fixed zero-latency
quality decision is [ADR 0002](docs/adr/0002-density-quality-latency.md); and the
independent validation boundary is
[ADR 0003](docs/adr/0003-independent-vst3-validator.md).

## Repository map

```text
apps/
  density/             production DSP, JUCE adapter, and editor
  dsp-lab/             offline renderer and measurement laboratory
  standalone-host/     minimal headless VST3 smoke host
docs/
  adr/                 accepted engineering decisions
  products/density/    cycle reports and Density research evidence
  research/            lawful historical-reference metadata
  testing/             listening protocols
tests/
  golden/              reviewed production metrics
  density_tests.cpp    framework-independent tests
  density_plugin_tests.cpp
  realtime_audit_mac.cpp
tools/                  repository-policy and provenance checks
```

The repo deliberately does **not** contain a speculative universal audio
framework, empty libraries for future products, or direct product-to-product
dependencies.

## Research and claims

Historical compressor, limiter, amplifier, and clipping circuits are studied
for engineering ideas such as headroom, loading, time constants, feedback,
recovery, and overload behaviour. The project does not redistribute service
manuals with unclear rights, copy proprietary firmware, or present schematic
study as measured hardware emulation.

Research metadata and access status are recorded in
[docs/research/schematics/catalog.yaml](docs/research/schematics/catalog.yaml).
The policy is in [SCHEMATIC_RESEARCH.md](SCHEMATIC_RESEARCH.md), and the broader
measurement standard is in [DSP_RESEARCH.md](DSP_RESEARCH.md).

## Documentation index

| Document | Purpose |
|---|---|
| [ARCHITECTURE.md](ARCHITECTURE.md) | Product boundaries, signal graph, and ownership |
| [docs/PRODUCT_PHILOSOPHY.md](docs/PRODUCT_PHILOSOPHY.md) | Family character and design principles |
| [PARAMETER_MODEL.md](PARAMETER_MODEL.md) | Stable IDs, ranges, defaults, mappings, and smoothing |
| [STATE_FORMAT.md](STATE_FORMAT.md) | Schema, validation, and state guarantees |
| [REALTIME_SAFETY.md](REALTIME_SAFETY.md) | Audio-thread policy and runtime audits |
| [TESTING.md](TESTING.md) | Test layers, lab protocols, and measurement tolerances |
| [HOST_COMPATIBILITY.md](HOST_COMPATIBILITY.md) | Validator and host evidence |
| [PERFORMANCE_BUDGETS.md](PERFORMANCE_BUDGETS.md) | CPU, memory, latency, and UI budgets |
| [UI_SYSTEM.md](UI_SYSTEM.md) | Shared visual and interaction language |
| [RELEASE_CHECKLIST.md](RELEASE_CHECKLIST.md) | All 25 Density release gates |
| [ROADMAP.md](ROADMAP.md) | Current sequencing and explicit deferrals |
| [RESEARCH_LOG.md](RESEARCH_LOG.md) | Research chronology |
| [CONTRIBUTING.md](CONTRIBUTING.md) | Change requirements |
| [SECURITY.md](SECURITY.md) | Prototype security reporting policy |

Each substantial engineering cycle records what changed, why, evidence, risks,
and the smallest next action under
[`docs/products/density/`](docs/products/density/).

## Contributing

Density is the only implementation priority until its release gates close.
Changes should identify a concrete musical or engineering failure, preserve the
DSP/UI/state boundaries, add the smallest deterministic regression check, and
include before/after measurements for DSP work. Golden data is never updated
automatically. Read [CONTRIBUTING.md](CONTRIBUTING.md) before opening a change.

## Licensing and distribution

No project-wide distribution licence has been selected, so this repository is
not itself permission to redistribute source or binaries.

The opt-in VST3 build fetches JUCE 8.0.13. JUCE is available under its EULA or
AGPLv3; a closed-source distributable requires the applicable JUCE licence and
reviewed notices. No third-party manuals, firmware, impulse responses, samples,
or commercial plugin code are included. See
[THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md) and
[LICENSES/README.md](LICENSES/README.md).

Do not distribute a plugin build until the company identity, project licence,
JUCE licence, signing identity, notarization, package contents, and release
gates have all been reviewed.
