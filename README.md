# Aste Signal Instruments

[![CI](https://github.com/yairfalse/aste/actions/workflows/ci.yml/badge.svg)](https://github.com/yairfalse/aste/actions/workflows/ci.yml)

`Aste` is the neutral internal codename for a planned family of professional
audio instruments. It is not the company name and must not become a permanent
public namespace, bundle identifier, preset signature, or installer path.

The project combines original DSP, restrained industrial interfaces, historical
engineering research, deterministic measurement, and unusually strict
real-time testing. It is not a hardware-cloning exercise and does not use
“analog” as a substitute for describing measurable behaviour.

> **Project status:** Density D-01, Harmonic H-01, Sequence S-01, Loop L-01,
> Impulse I-01, and Field F-01 are complete working internal VST3 prototypes.
> All six have
> product-owned DSP, state, tests, industrial UIs, and universal macOS bundles.
> The local and hosted 67-test, sanitizer, universal signature/slice, ABI-host,
> and Steinberg extensive-validation gates pass for the six-product line. None
> is a supported release: Developer ID
> signing, notarization, final company identity, and DAW compatibility remain open.

## The instrument family

| Product | Purpose | Status |
|---|---|---|
| **Density D-01** | Parallel hard mastering compressor | Complete internal prototype; DAW validation next |
| **Harmonic H-01** | Equalizer with nonlinear band behaviour | Complete internal prototype; musical/DAW validation next |
| **Sequence S-01** | Monophonic programmed-current synthesizer | Complete internal prototype; musical/DAW validation next |
| **Loop L-01** | Tape-inspired playable memory instrument | Complete internal prototype; captured-memory recall next |
| **Impulse I-01** | Generative rhythm and transient instrument | Complete internal prototype; musical/DAW validation next |
| **Field F-01** | Playable reverb and stored-spatial-energy instrument | Complete internal prototype; musical/DAW validation next |

All six products remain independent. Shared code is extracted only when two
products genuinely need identical behavior or when correctness requires one
implementation; no product processor links to another product.

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
| Drive | -12 to +24 dB | 0 dB | Pushes the crush audio, detector, and positive-drive saturation |
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
see [PARAMETER_MODEL.md](docs/PARAMETER_MODEL.md).

### Sound and interface

At restrained settings Density is intended to increase presence and continuity
while keeping the transient-bearing dry path intact. Crush now also controls
nonlinear participation, while positive Drive pushes saturation by up to 75%.
At stronger settings the expanded -30 dB / 60:1 macro range becomes
deliberately hard, close, and physical. The output stage is protective
sample-peak clipping, **not** a true-peak mastering limiter.

The current 980×540 editor scales from 760×420 to 1520×840. It uses a near-black
surface, off-white technical typography, a muted burgundy accent, explicit
keyboard order, editable values, and signal-driven meters. There are no tabs,
fake materials, rack decorations, or hidden essential controls. The full family
language is documented in [UI_SYSTEM.md](docs/UI_SYSTEM.md).

## Harmonic H-01

Harmonic is a four-band broad equalizer whose boost behavior includes a bounded
dynamic nonlinear component. Cuts stay clean; boosts progressively add a
stateful harmonic contribution without switching topology. Its revised
square-root boost participation makes even modest positive boosts respond,
while the full Harmonic range reaches an intentionally obvious nonlinear
region. The serial graph is:

```text
input -> Foundation -> Body -> Presence -> Air -> output
           clean cuts / nonlinear-participating boosts
```

Foundation, Body, Presence, and Air each expose gain and a constrained frequency
range. Input, Harmonic, and Output are global. The Harmonic macro continuously
coordinates the bounded-stage drive and contribution; it does not add noise,
drift, or a generic post-EQ saturator. The graph is minimum-phase, uses no
lookahead or oversampling, and reports zero latency.

Candidate 3 is deliberately provisional. It passed the laboratory contour,
harmonic, IMD, recovery, finite-output, and CPU gates needed to justify a real
plugin, but the music-machine test decides whether its four-band interaction is
actually useful. The stable internal-beta contract is in
[docs/products/harmonic/PARAMETERS.md](docs/products/harmonic/PARAMETERS.md), and
the topology decision is [ADR 0007](docs/adr/0007-harmonic-internal-beta-topology.md).

## Sequence S-01

Sequence is a monophonic programmed-current synthesizer with a directly visible
16-step host-synchronised program. Each step owns note offset, gate, accent, and
slide. MIDI plays the voice while stopped and transposes the pattern while the
host runs.

Its original voice combines two anti-aliased oscillators and a sub oscillator,
a saw–pulse–sine wave path, pulse-width control, a bounded driven mixer, and one
nonlinear four-stage filter whose Weight control moves continuously between its
two- and four-pole outputs. This shared-state topology avoids the level and
phase hollowing of parallel-filter crossfades. Pressure coordinates mixer
drive, filter-envelope depth, accent gain, and filter loading. It is
topology-informed, not an emulation of a named synthesizer.

The full internal-beta contract is
[docs/products/sequence/SPECIFICATION.md](docs/products/sequence/SPECIFICATION.md).

## Loop L-01

Loop catches mono or stereo audio on three preallocated tape decks. RELOOP
prints the currently transformed playback through a record/reproduce stage onto
the next deck, allowing pitch, reverse, splice, transport motion, loading, and
loss to accumulate across generations. The latest three generations remain
navigable while the generation counter continues. Host-synced beat length and
free seconds remain separate stable parameters; MIDI performance actions are
sample-offset accurate.

The prototype reports zero latency and never allocates or locks while
processing. Its schema-2 control state and schema-1 migration are deterministic,
but captured audio and generations are not yet serialized; session memory
recall remains a stated release blocker. See
[docs/products/loop/SPECIFICATION.md](docs/products/loop/SPECIFICATION.md).

## Impulse I-01

Impulse synthesizes eight objects—Kick, Click, Burst, Body, Low, Crack, Metal,
and Cut—from exciters, resonant bodies, and bounded amplifier stages. Every
track owns an always-visible 32-step Off/Hit/Accent row with an independent
1–32-step cycle, probability, ratchets, timing, condition, and accent. Pulses
and Rotation are an explicit pattern generator, never a hidden playback source.
Stored seeds make probability, variation, and mutation reproducible from host
PPQ across block partitions and project recall. MIDI pitch classes C through G
trigger the eight objects directly.

The default polymetric lengths remain host-clocked while drifting against one
another. No drum samples, genre kits, named circuit clone, or nondeterministic
“analog” behavior defines the product. See
[docs/products/impulse/SPECIFICATION.md](docs/products/impulse/SPECIFICATION.md).

## Field F-01

Field is a heavy reverb conceived as stored spatial energy rather than a room
simulation. Its eight moving feedback lines, deterministic grain motion, and
two internal dual-head pitch voices form one recirculating instrument. Audio or
sample-offset MIDI notes can excite the field. The large **FOREVER** control
smoothly holds and animates its memory; press it again to release back into the
Mass-controlled decay.

Mass, Grain, Pitch, Motion, Distance, Blend, and Output remain on one panel.
There is no IR, named room, hidden quality page, decorative animation,
nondeterministic drift, or hardware-emulation claim. Field reports zero latency
and its live audio memory intentionally starts empty after session reload. See
[docs/products/field/SPECIFICATION.md](docs/products/field/SPECIFICATION.md).

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
  generator;
- `harmonic_dsp` and `harmonic_tests` — independent four-band production DSP and
  its correctness/property checks;
- `harmonic_lab` — candidate research, six-rate product reports, and the
  production-graph benchmark.
- `sequence_dsp` and `sequence_tests` — independent MIDI voice, host-clocked
  step program, filters, and correctness properties.
- `sequence_lab` — deterministic host-clocked render and worst-case voice
  benchmark.
- `loop_dsp`, `loop_tests`, and `loop_lab` — capture/playback memory engine,
  property tests, deterministic render, and worst-case benchmark.
- `impulse_dsp`, `impulse_tests`, and `impulse_lab` — physical-event voices,
  deterministic polymetric scheduling, render, and benchmark.
- `field_dsp`, `field_tests`, and `field_lab` — stored-energy field, grain and
  pitch feedback, six-rate report, and worst-case benchmark.

### VST3, adapter tests, and standalone host

The VST3 build is opt-in because it fetches licensed JUCE source:

```sh
cmake -S . -B build-plugin \
  -DASTE_BUILD_VST3=ON \
  -DCMAKE_BUILD_TYPE=Release
cmake --build build-plugin --parallel
ctest --test-dir build-plugin --output-on-failure
```

The plugin bundles are produced at:

```text
build-plugin/DensityD01_artefacts/Release/VST3/Density D-01.vst3
build-plugin/HarmonicH01_artefacts/Release/VST3/Harmonic H-01.vst3
build-plugin/SequenceS01_artefacts/Release/VST3/Sequence S-01.vst3
build-plugin/LoopL01_artefacts/Release/VST3/Loop L-01.vst3
build-plugin/ImpulseI01_artefacts/Release/VST3/Impulse I-01.vst3
build-plugin/FieldF01_artefacts/Release/VST3/Field F-01.vst3
```

Each product has direct JUCE-adapter tests. The separate `vst3_smoke_host`
links no product adapter or DSP; it discovers and loads each built bundle
through the actual VST3 ABI, restores state, and processes irregular blocks.

To run that smoke host directly:

```sh
"build-plugin/vst3_smoke_host_artefacts/Release/vst3_smoke_host" \
  "build-plugin/HarmonicH01_artefacts/Release/VST3/Harmonic H-01.vst3" \
  "Harmonic H-01" "Harmonic" 12
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
cmake -S . -B build-plugin-universal \
  -DASTE_BUILD_VST3=ON \
  -DCMAKE_BUILD_TYPE=Release \
  '-DCMAKE_OSX_ARCHITECTURES=arm64;x86_64'
cmake --build build-plugin-universal --parallel
ctest --test-dir build-plugin-universal --output-on-failure
```

The internal bundle receives an ad-hoc signature so its completed manifest and
binary can be validated locally. It is not Developer ID signed or notarized.

### Build and install all instruments on a music Mac

Clone the repository on the music machine, quit Cubase and Ableton, then run:

```sh
./tools/build_and_install_macos.sh
```

The script builds and tests universal arm64+x86_64 bundles, verifies both
architectures and signatures, and installs the complete six-product line under
`~/Library/Audio/Plug-Ins/VST3`. Existing copies are preserved as timestamped
backups, and no `sudo` is used. Reopen the DAW and rescan VST3 plugins.

These are ad-hoc-signed, unnotarized internal builds with deliberately invalid
placeholder bundle identifiers. macOS may require locally allowing the bundle;
they are for private host and musical validation, not distribution.

For the plain transfer-folder workflow, see
[INSTALL_MACOS.md](docs/INSTALL_MACOS.md).

An explicit bundle path may be supplied when testing another build:

```sh
./tools/install_density_macos.sh \
  "build-plugin-universal/DensityD01_artefacts/Release/VST3/Density D-01.vst3"
./tools/install_harmonic_macos.sh \
  "build-plugin-universal/HarmonicH01_artefacts/Release/VST3/Harmonic H-01.vst3"
./tools/install_sequence_macos.sh \
  "build-plugin-universal/SequenceS01_artefacts/Release/VST3/Sequence S-01.vst3"
./tools/install_loop_macos.sh \
  "build-plugin-universal/LoopL01_artefacts/Release/VST3/Loop L-01.vst3"
./tools/install_impulse_macos.sh \
  "build-plugin-universal/ImpulseI01_artefacts/Release/VST3/Impulse I-01.vst3"
./tools/install_field_macos.sh \
  "build-plugin-universal/FieldF01_artefacts/Release/VST3/Field F-01.vst3"
```

### Internal packaging rehearsal

After building the universal tree, create and inspect the deterministic internal
archive with:

```sh
cmake --build build-plugin-universal --target density_package
python3 tools/package_density.py verify \
  --archive build-plugin-universal/packages/Density-D01-0.1.0-internal-macos-universal.zip
```

The archive is explicitly marked `internal-development-only`. It is ad-hoc
signed, uses the placeholder bundle identity, is not notarized, and must not be
distributed. It includes a deterministic SPDX 2.3 SBOM for Density, pinned JUCE,
JUCE's bundled VST3 SDK, and the exact reviewed dependency-security ledger.
`PACKAGE.json` binds the ledger's SHA-256, review date, expiry, and disposition;
normal inspection rejects expired or altered evidence. The format decisions are
[ADR 0004](docs/adr/0004-internal-package-format.md) and
[ADR 0005](docs/adr/0005-spdx-sbom.md).

The packaged dependency pins also have a dated, expiring advisory review:

```sh
python3 tools/check_dependency_security.py \
  docs/security/dependency-audit.json .
```

The check is offline and runs under CTest. Its evidence sources and limits are
defined by
[ADR 0006](docs/adr/0006-expiring-dependency-security-review.md).

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
[TESTING.md](docs/TESTING.md); algorithm decisions and rejected candidates live in
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

The plugin test tree runs the complete deterministic CTest suite. Independent local
validation has passed pluginval 1.0.4 at strictness 10 and the Steinberg VST3 SDK
3.8.0 extensive suite at 537/537. Density has arm64 and Rosetta x86_64 validator
history; Harmonic's current universal beta has arm64 validator evidence and
separate two-slice/ABI verification. Sequence has direct core, adapter, and
repository ABI-host evidence; independent validator and DAW runs remain next.
The exact run history is in [HOST_COMPATIBILITY.md](docs/HOST_COMPATIBILITY.md).

GitHub Actions builds the core on arm64 and Intel runners, builds a universal
VST3, verifies its architectures and ad-hoc signature, and runs an arm64
sanitizer tree. It also builds the pinned official Steinberg validator from
source and requires its extensive suite to pass the signed bundle. The workflow
is [`.github/workflows/ci.yml`](.github/workflows/ci.yml).

### Real-time rules

All production processors expose `noexcept` processing. Processing may not allocate, free,
lock, wait, log, access files or networks, resize containers, trigger lazy
initialization, call blocking OS services, or throw through the callback.
Buffers and mutable DSP state are prepared in advance. Non-finite input is
sanitized and output must remain finite. Thread ownership and the limits of the
runtime audit are documented in [REALTIME_SAFETY.md](docs/REALTIME_SAFETY.md).

### State

Each product uses its own portable, versioned XML state document containing
stable parameter IDs and scalar values. Unknown parameters are ignored, missing
parameters receive defaults, duplicate IDs are rejected, and malformed state
leaves the current configuration unchanged. See [STATE_FORMAT.md](docs/STATE_FORMAT.md).

### Performance

The current production graph measured a 0.283874% median of one M4 Pro
performance core at 48 kHz / 128 samples under worst-case block-rate automation.
This is comfortably below the provisional 1% target on that machine, but it is
not evidence for the oldest supported Apple Silicon or a native Intel Mac. Full
measurements and rejected oversampling budgets are in
[PERFORMANCE_BUDGETS.md](docs/PERFORMANCE_BUDGETS.md).

Harmonic measured 0.718134% on the same class of machine at 48 kHz / 128
samples while every band and its macro changed each block. This passes the
local provisional 1% budget, but still requires older Apple Silicon and native
Intel evidence.

Sequence measured 0.292424% on the same machine at 48 kHz / 127 samples with
Pressure at 100%, resonance at 85%, the shared-state character filter active,
and stereo output. It passes the local provisional 1% budget; native Intel,
older Apple Silicon, and DAW/UI-open measurements remain external gates.

Loop's generational topology measured a 0.526622% five-run median on the same
machine at 48 kHz / 127 samples while continuously printing through pitch,
transport modulation, Record, and Loss at worst case. It passes the local
provisional 1% budget; captured-memory recall, native Intel, older Apple
Silicon, and DAW/UI-open measurements remain open.

Impulse measured a 1.26032% five-run median on the same machine at 48 kHz /
127 samples with all eight objects and every visible pattern cell active, 4×
ratchets, 180 BPM, maximum Energy, Variation, Mutation, and Drive. It passes
its 2% eight-object stress budget; native Intel, older Apple Silicon, DAW, and
open-UI evidence remain external.

Field's five-run worst-case Release benchmark measured a 0.320727% median of
one M4 Pro performance core at 48 kHz / 127 samples with FOREVER, Mass, Grain,
Pitch, Motion, Distance, and Blend at maximum. Its fixed processing state is
1,180,056 bytes, below the product-specific 1.5 MiB budget. Native Intel,
oldest-supported Apple Silicon, loaded-DAW, long-held-energy, and open-UI
measurements remain external.

## Current release status

The six-product line consists of internal prototypes, not releases. Density
has repository evidence for 16 of its 25 release gates. Harmonic has the
engineering boundary needed for first external tests; all six still require
people, DAWs, and target hardware for:

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

The authoritative ledger is [RELEASE_CHECKLIST.md](docs/RELEASE_CHECKLIST.md), and
the next permitted work is in [ROADMAP.md](docs/ROADMAP.md). Do not turn a green build
into a compatibility or fidelity claim.

## Architecture

```text
density_dsp  <- density_tests  <- density_lab
      ^
      +------ Density JUCE adapter/UI <- density_plugin_tests

harmonic_dsp <- harmonic_tests <- harmonic_lab
      ^
      +------ Harmonic JUCE adapter/UI <- harmonic_plugin_tests

sequence_dsp <- sequence_tests
      ^
      +------ Sequence JUCE instrument/UI <- sequence_plugin_tests

loop_dsp     <- loop_tests <- loop_lab
      ^
      +------ Loop JUCE effect/UI <- loop_plugin_tests

impulse_dsp  <- impulse_tests <- impulse_lab
      ^
      +------ Impulse JUCE instrument/UI <- impulse_plugin_tests

field_dsp    <- field_tests <- field_lab
      ^
      +------ Field JUCE effect/UI <- field_plugin_tests

all VST3 bundles <- vst3_smoke_host (dynamic ABI load only)
```

The production DSP is ordinary C++20. JUCE is restricted to the VST3, host,
state-adapter, and UI boundary. Product DSP does not depend on widgets, and
serialized state does not depend on widget layouts. The accepted framework
decision is [ADR 0001](docs/adr/0001-plugin-framework.md); the fixed zero-latency
quality decision is [ADR 0002](docs/adr/0002-density-quality-latency.md); and the
independent validation boundary is
[ADR 0003](docs/adr/0003-independent-vst3-validator.md); and the internal-only
package boundary is [ADR 0004](docs/adr/0004-internal-package-format.md).
The package dependency boundary is [ADR 0005](docs/adr/0005-spdx-sbom.md).
The expiring dependency-security review is
[ADR 0006](docs/adr/0006-expiring-dependency-security-review.md).
Field's fixed stored-energy topology and memory exception are
[ADR 0015](docs/adr/0015-field-stored-energy-topology.md).

## Repository map

```text
apps/
  density/             production DSP, JUCE adapter, and editor
  harmonic/            independent four-band DSP, adapter, and editor
  sequence/            monophonic voice, host sequencer, adapter, and editor
  loop/                capture memory, adapter, and editor
  impulse/             rhythmic objects, polymetric clock, adapter, and editor
  field/               moving feedback field, pitch memory, adapter, and editor
  dsp-lab/             offline renderer and measurement laboratory
  standalone-host/     minimal headless VST3 smoke host
docs/
  adr/                 accepted engineering decisions
  products/density/    cycle reports and Density research evidence
  products/harmonic/   Harmonic contract, research, and test handoff
  products/field/      Field contract, state, cycle evidence, and test handoff
  research/            lawful historical-reference metadata
  testing/             listening protocols
tests/
  golden/              reviewed production metrics
  density_tests.cpp    framework-independent tests
  density_plugin_tests.cpp
  harmonic_tests.cpp
  harmonic_plugin_tests.cpp
  sequence_tests.cpp / sequence_plugin_tests.cpp
  loop_tests.cpp / loop_plugin_tests.cpp
  impulse_tests.cpp / impulse_plugin_tests.cpp
  field_tests.cpp / field_plugin_tests.cpp
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
The policy is in [SCHEMATIC_RESEARCH.md](docs/SCHEMATIC_RESEARCH.md), and the broader
measurement standard is in [DSP_RESEARCH.md](docs/DSP_RESEARCH.md).

## Documentation index

| Document | Purpose |
|---|---|
| [ARCHITECTURE.md](docs/ARCHITECTURE.md) | Product boundaries, signal graph, and ownership |
| [docs/PRODUCT_PHILOSOPHY.md](docs/PRODUCT_PHILOSOPHY.md) | Family character and design principles |
| [PARAMETER_MODEL.md](docs/PARAMETER_MODEL.md) | Stable IDs, ranges, defaults, mappings, and smoothing |
| [STATE_FORMAT.md](docs/STATE_FORMAT.md) | Schema, validation, and state guarantees |
| [REALTIME_SAFETY.md](docs/REALTIME_SAFETY.md) | Audio-thread policy and runtime audits |
| [TESTING.md](docs/TESTING.md) | Test layers, lab protocols, and measurement tolerances |
| [HOST_COMPATIBILITY.md](docs/HOST_COMPATIBILITY.md) | Validator and host evidence |
| [PERFORMANCE_BUDGETS.md](docs/PERFORMANCE_BUDGETS.md) | CPU, memory, latency, and UI budgets |
| [UI_SYSTEM.md](docs/UI_SYSTEM.md) | Shared visual and interaction language |
| [RELEASE_CHECKLIST.md](docs/RELEASE_CHECKLIST.md) | All 25 Density release gates |
| [SPECIFICATION_STATUS.md](docs/SPECIFICATION_STATUS.md) | Master-brief requirements mapped to evidence, decisions, and external gates |
| [ROADMAP.md](docs/ROADMAP.md) | Current sequencing and explicit deferrals |
| [RESEARCH_LOG.md](docs/RESEARCH_LOG.md) | Research chronology |
| [CONTRIBUTING.md](docs/CONTRIBUTING.md) | Change requirements |
| [SECURITY.md](docs/SECURITY.md) | Prototype security reporting policy |

Each substantial engineering cycle records what changed, why, evidence, risks,
and the smallest next action in its product documentation directory.

## Contributing

Changes should identify a concrete musical or engineering failure, preserve the
DSP/UI/state boundaries, add the smallest deterministic regression check, and
include before/after measurements for DSP work. Golden data is never updated
automatically. Read [CONTRIBUTING.md](docs/CONTRIBUTING.md) before opening a change.

## Licensing and distribution

No project-wide distribution licence has been selected, so this repository is
not itself permission to redistribute source or binaries.

The opt-in VST3 build fetches JUCE 8.0.13. JUCE is available under its EULA or
AGPLv3; a closed-source distributable requires the applicable JUCE licence and
reviewed notices. No third-party manuals, firmware, impulse responses, samples,
or commercial plugin code are included. See
[THIRD_PARTY_NOTICES.md](docs/THIRD_PARTY_NOTICES.md) and
[LICENSES/README.md](LICENSES/README.md).

Do not distribute a plugin build until the company identity, project licence,
JUCE licence, signing identity, notarization, package contents, and release
gates have all been reviewed.
